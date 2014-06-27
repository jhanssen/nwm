#include "ScriptEngine.h"
#define MOZJS_USE_JSIDARRAY
#include <jsapi.h>
#include <js/Value.h>
#include <rct/EventLoop.h>

static Value toRct(JSContext *context, JS::Value *value);
static jsval fromRct(JSContext *context, const Value &value);

#if 0
static JSContext *
NewContext(JSRuntime *mRuntime)
{
    JSContext *mContext;
    WITH_SIGNALS_DISABLED(mContext = JS_NewContext(mRuntime, gStackChunkSize));
    if (!mContext)
        return NULL;

    JSShellContextData *data = NewContextData();
    if (!data) {
        DestroyContext(mContext, false);
        return NULL;
    }

    JS_SetContextPrivate(mContext, data);
    JS_SetErrorReporter(mContext, my_ErrorReporter);
    SetContextOptions(mContext);
    if (enableTypeInference)
        JS_ToggleOptions(mContext, JSOPTION_TYPE_INFERENCE);
    if (enableIon)
        JS_ToggleOptions(mContext, JSOPTION_ION);
    if (enableBaseline)
        JS_ToggleOptions(mContext, JSOPTION_BASELINE);
    if (enableAsmJS)
        JS_ToggleOptions(mContext, JSOPTION_ASMJS);
    return mContext;
}

static void
DestroyContext(JSContext *mContext, bool withGC)
{
    JSShellContextData *data = GetContextData(mContext);
    JS_SetContextPrivate(mContext, NULL);
    free(data);
    WITH_SIGNALS_DISABLED(withGC ? JS_DestroyContext(mContext) : JS_DestroyContextNoGC(mContext));
}

static JSObject *
NewGlobalObject(JSContext *mContext, JSObject *sameZoneAs)
{
    CompartmentOptions options;
    options.setZone(sameZoneAs ? SameZoneAs(sameZoneAs) : FreshZone)
    .setVersion(JSVERSION_LATEST);
    RootedObject glob(mContext, JS_NewGlobalObject(mContext, &global_class, NULL, options));
    if (!glob)
        return NULL;

    {
        JSAutoCompartment ac(mContext, glob);

#ifndef LAZY_STANDARD_CLASSES
        if (!JS_InitStandardClasses(mContext, glob))
            return NULL;
#endif

#ifdef JS_HAS_CTYPES
        if (!JS_InitCTypesClass(mContext, glob))
            return NULL;
#endif
        if (!JS_InitReflect(mContext, glob))
            return NULL;
        if (!JS_DefineDebuggerObject(mContext, glob))
            return NULL;
        if (!RegisterPerfMeasurement(mContext, glob))
            return NULL;
        if (!JS_DefineFunctionsWithHelp(mContext, glob, shell_functions) ||
            !JS_DefineProfilingFunctions(mContext, glob)) {
            return NULL;
        }
        if (!DefineTestingFunctions(mContext, glob))
            return NULL;

        if (!fuzzingSafe && !JS_DefineFunctionsWithHelp(mContext, glob, fuzzing_unsafe_functions))
            return NULL;

        RootedObject it(mContext, JS_DefineObject(mContext, glob, "it", &its_class, NULL, 0));
        if (!it)
            return NULL;
        if (!JS_DefineProperties(mContext, it, its_props))
            return NULL;

        if (!JS_DefineProperty(mContext, glob, "custom", UndefinedValue(), its_getter,
                               its_setter, 0))
            return NULL;
        if (!JS_DefineProperty(mContext, glob, "customRdOnly", UndefinedValue(), its_getter,
                               its_setter, JSPROP_READONLY))
            return NULL;

        if (!JS_DefineProperty(mContext, glob, "customNative", UndefinedValue(),
                               JS_CAST_NATIVE_TO(its_get_customNative, JSPropertyOp),
                               JS_CAST_NATIVE_TO(its_set_customNative, JSStrictPropertyOp),
                               JSPROP_SHARED | JSPROP_NATIVE_ACCESSORS)) {
            return NULL;
        }

        /* Initialize FakeDOMObject. */
        static DOMCallbacks DOMcallbacks = {
            InstanceClassHasProtoAtDepth
        };
        SetDOMCallbacks(mContext->runtime(), &DOMcallbacks);

        RootedObject domProto(mContext, JS_InitClass(mContext, glob, NULL, &dom_class, dom_constructor, 0,
                              dom_props, dom_methods, NULL, NULL));
        if (!domProto)
            return NULL;

        /* Initialize FakeDOMObject.prototype */
        InitDOMObject(domProto);
    }

    return glob;
}
#endif

static void errorReporter(JSContext */* context */, const char *message, JSErrorReport *report)
{
    error() << message;
    // gGotError = PrintError(mContext, gErrFile, message, report, reportWarnings);
    if (!JSREPORT_IS_WARNING(report->flags))
        EventLoop::eventLoop()->quit();
    //     if (report->errorNumber == JSMSG_OUT_OF_MEMORY) {
    //         gExitCode = EXITCODE_OUT_OF_MEMORY;
    //     } else {
    //         gExitCode = EXITCODE_RUNTIME_ERROR;
    //     }
    // }
}

ScriptEngine *ScriptEngine::sInstance = 0;
ScriptEngine::ScriptEngine()
{
    assert(!sInstance);
    sInstance = this;
    mRuntime = JS_NewRuntime(32L * 1024L * 1024L, JS_USE_HELPER_THREADS);
    if (!mRuntime) {
        error() << "Failed to initialize JS engine runtime";
        abort();
    }

    mContext = JS_NewContext(mRuntime, 8192);
    if (!mContext) {
        error() << "Failed to initialize JS engine context";
        abort();
    }

    // JSShellContextData *data = NewContextData();
    // if (!data) {
    //     DestroyContext(mContext, false);
    //     return NULL;
    // }

    JS_SetContextPrivate(mContext, this);
    JS_SetErrorReporter(mContext, errorReporter);

    JSAutoRequest autoRequest(mContext);
    JSClass globalClass = {
        "global",
        JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
        JS_PropertyStub,
        JS_DeletePropertyStub,
        JS_PropertyStub,
        JS_StrictPropertyStub,
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        nullptr,
        JSCLASS_NO_OPTIONAL_MEMBERS
    };

    JSObject *glob = JS_NewGlobalObject(mContext, &globalClass, 0);
    if (!JS_InitStandardClasses(mContext, glob)) {
        error() << "Failed to initialize standard classes";
        abort();
    }
    mGlobalObject = new Object(glob);
    //     result = Shell(mContext, &op, envp);

    // #ifdef DEBUG
    //     if (OOM_printAllocationCount)
    //         printf("OOM max count: %u\n", OOM_counter);
    // #endif

    //     gTimeoutFunc = NullValue();
    //     JS_RemoveValueRootRT(mRuntime, &gTimeoutFunc);

    //     DestroyContext(mContext, true);

    //     KillWatchdog();

    //     JS_DestroyRuntime(mRuntime);
    //     JS_ShutDown();

    //     if (!JS_Init()) {
    //     }
    //     mRuntime = JS_NewRuntime(8L * 1024L * 1024L, JS_NO_HELPER_THREADS);
}

ScriptEngine::~ScriptEngine()
{
    assert(sInstance == this);
    sInstance = 0;
    delete mGlobalObject;
}

void ScriptEngine::throwError(const String &error)
{
    JSAutoRequest autoRequest(mContext);
    JS_ReportError(mContext, "%s", error.constData());
}

class ObjectPrivate
{
public:
    ObjectPrivate(JSObject *object)
        : mObject(ScriptEngine::instance()->context()) {
        mObject = object ? object : JS_NewObject(ScriptEngine::instance()->context(), 0, 0, 0);
    }
    ~ObjectPrivate() {
        for (const auto &it : mMembers)
            delete it.second;
    }
    struct Member {
        enum Type {
            Type_Property,
            Type_PropertyReadOnly,
            Type_Function,
            Type_Child
        } type;
        ScriptEngine::Object::Function function;
        struct {
            ScriptEngine::Object::Setter setter;
            ScriptEngine::Object::Getter getter;
        } property;
        ScriptEngine::Object *child;

        Member(ScriptEngine::Object *c)
            : type(Type_Child), child(c) {
        }

        Member(ScriptEngine::Object::Getter &&getter)
            : type(Type_PropertyReadOnly), child(0) {
            property.getter = std::move(getter);
        }
        Member(ScriptEngine::Object::Getter &&getter, ScriptEngine::Object::Setter &&setter)
            : type(Type_Property), child(0) {
            property.getter = std::move(getter);
            property.setter = std::move(setter);
        }
        Member(ScriptEngine::Object::Function &&func)
            : type(Type_Function), function(std::move(func)), child(0) {
        }

        ~Member() {
            delete child;
        }
    };

    JS::RootedObject mObject;
    Hash<String, Member*> mMembers;
};

ScriptEngine::Object::Object(JSObject *object)
    : mPrivate(new ObjectPrivate(object))
{
}

ScriptEngine::Object::~Object()
{
    delete mPrivate;
}

static JSBool jsCallback(JSContext *context, unsigned argc, JS::Value *vp)
{
    JS::CallArgs callArgs = JS::CallArgsFromVp(argc, vp);
    JSObject &callee = callArgs.callee();
    ObjectPrivate::Member *member = static_cast<ObjectPrivate::Member*>(JS_GetPrivate(&callee));
    if (!member) {
        JS_ReportError(context, "Invalid function called");
        return false;
    }
    assert(member);
    const int length = callArgs.length();
    List<Value> args(length);
    for (int i=0; i<length; ++i)
        args[i] = toRct(context, &callArgs[i]);
    const Value retVal = member->function(args);
    callArgs.rval().set(fromRct(context, retVal));
    return true;
}

void ScriptEngine::Object::registerFunction(const String &name, Function &&func)
{
    ObjectPrivate::Member *&member = mPrivate->mMembers[name];
    assert(!member);
    JSContext *context = ScriptEngine::instance()->context();
    JSFunction *jsFunc = JS_DefineFunction(context, mPrivate->mObject, name.constData(), jsCallback, 0, JSPROP_ENUMERATE);
    if (jsFunc) {
        JSObject *obj = JS_GetFunctionObject(jsFunc);
        if (obj) {
            member = new ObjectPrivate::Member(std::move(func));
            JS_SetPrivate(obj, member);
        }
    }
}

void ScriptEngine::Object::registerProperty(const String &name, Getter &&get)
{

}

void ScriptEngine::Object::registerProperty(const String &name, Getter &&get, Setter &&set)
{

}

ScriptEngine::Object *ScriptEngine::Object::child(const String &name)
{
    ObjectPrivate::Member *&data = mPrivate->mMembers[name];
    if (!data) {
        data = new ObjectPrivate::Member(new Object);
    }
    assert(data->type == ObjectPrivate::Member::Type_Child);
    return data->child;
}

static inline String convertToString(JSContext *context, JSString *str)
{
    JS::Anchor<JSString *> a_str(str);
    return String(JS_EncodeString(context, str), JS_GetStringEncodingLength(context, str));
}

// static inline String convertToString(JSContext *context, JS::Value *value)
// {
//     String result;
//     if (value->isString()) {
//         result = convertToString(context, value->toString());
//     }
//     return result;
// }

static Value toRct(JSContext *context, JS::Value *value)
{
    Value result;
    if (value->isString()) {
        result = convertToString(context, value->toString());
    } else if (value->isInt32()) {
        result = value->toInt32();
    } else if (value->isDouble()) {
        result = value->toDouble();
    } else if (value->isBoolean()) {
        result = value->toBoolean();
    } else if (value->isObject()) {
        JSObject *object = &value->toObject();
        JS::Anchor<JSObject *> a_obj(object);
        const bool isArray = JS_IsArrayObject(context, object);
        if (JSIdArray *properties = JS_Enumerate(context, object)) {
            JS::Value e;
            jsid property;
            List<Value> array;
            const int length = JS_IdArrayLength(context, properties);
            for (int i = 0; i < length; ++i) {
                property = JS_IdArrayGet(context, properties, i);
                if (JS_GetPropertyById(context, object, property, &e)) {
                    const Value v = toRct(context, &e);
                    if (isArray) {
                        array.push_back(v);
                    } else if (JSID_IS_STRING(property)) {
                        if (JSString *jsname = JSID_TO_STRING(property)) {
                            const std::string name(convertToString(context, jsname));
                            result[name] = v;
                        }
                    } else {
                        assert(false);
                    }
                }
            }
            JS_DestroyIdArray(context, properties);
            if (isArray)
                return array;
        }
        return result;

    }
    return result;
}

static JS::Value fromRct(JSContext *context, const Value &value)
{
    JS::Value result;
    switch (value.type()) {
    case Value::Type_String: {
        result = STRING_TO_JSVAL(JS_NewStringCopyZ(context, value.toString().constData()));
        break;
    }
    case Value::Type_List: {
        std::vector<jsval> v(value.count());
        const auto end = value.listEnd();
        int idx = 0;
        for (auto it = value.listBegin(); it != end; ++it)
            v[idx++] = fromRct(context, *it);

        result = OBJECT_TO_JSVAL(JS_NewArrayObject(context, v.size(), &v[0]));
        break;
    }
    case Value::Type_Map: {
        JSObject *o = JS_NewObject(context, 0, 0, 0);
        const auto end = value.end();
        for (auto it = value.begin(); it != end; ++it) {
            jsval v = fromRct(context, it->second);
            JS_SetProperty(context, o, it->first.constData(), &v);
        }
        result = OBJECT_TO_JSVAL(o);
        break;
    }
    case Value::Type_Integer:
        result = INT_TO_JSVAL(value.toInteger());
        break;
    case Value::Type_Double:
        result = DOUBLE_TO_JSVAL(value.toDouble());
        break;
    case Value::Type_Boolean:
        result = value.toBool() ? JSVAL_TRUE : JSVAL_FALSE;
        break;
    case Value::Type_Invalid:
    default:
        result = JSVAL_VOID;
        break;
    }
    return result;
}

