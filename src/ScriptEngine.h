#ifndef ScriptEngine_h
#define ScriptEngine_h

#include <rct/String.h>
#include <rct/Value.h>
#include <rct/Hash.h>
class JSObject;
class JSRuntime;
class JSContext;
namespace js {
class RootedObject;
}

class ObjectPrivate;
class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    static ScriptEngine *instance() { return sInstance; }

    JSContext *context() const { return mContext; }
    JSRuntime *runtime() const { return mRuntime; }
    void throwError(const String &error);

    class Object
    {
    public:
        ~Object();

        typedef std::function<Value (const List<Value> &)> Function;
        typedef std::function<Value ()> Getter;
        typedef std::function<void (const Value &)> Setter;

        void registerFunction(const String &name, Function &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);

        Object *child(const String &name);
    private:
        Object(JSObject *object = 0);

        ObjectPrivate *mPrivate;
        friend class ScriptEngine;
    };

    Object *globalObject() const { return mGlobalObject; }
private:
    static ScriptEngine *sInstance;
    JSRuntime *mRuntime;
    JSContext *mContext;
    Object *mGlobalObject;
};

#endif
