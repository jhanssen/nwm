#include "JavaScript.h"
#include "Util.h"
#include "WindowManager.h"
#include <rct/Log.h>

JavaScript::JavaScript()
    : ScriptEngine()
{
}


static inline Layout::SharedPtr parentOfFocus()
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (!wm)
        return Layout::SharedPtr();
    Client::SharedPtr current = Workspace::active()->focusedClient();
    if (!current)
        return Layout::SharedPtr();
    const Layout::SharedPtr& layout = current->layout();
    if (!layout)
        return Layout::SharedPtr();
    const Layout::SharedPtr& parent = layout->parent();
    if (!parent)
        return Layout::SharedPtr();
    return parent;
}

void JavaScript::init()
{
    auto global = globalObject();

    // --------------- console ---------------

    auto console = global->child("console");
    console->registerFunction("log", [](const List<Value>& args) -> Value {
            if (args.isEmpty()) {
                return ScriptEngine::instance()->throwException("No arguments passed to console.log");
            }
            const Log& log = error();
            for (const Value& arg : args) {
                const String& str = arg.toString();
                if (str.isEmpty())
                    log << arg;
                else
                    log << str;
            }
            return Value();
        });

    // --------------- nwm ---------------

    auto nwm = global->child("nwm");
    nwm->registerFunction("exec", [](const List<Value>& args) -> Value {
            if (args.size() != 1) {
                return ScriptEngine::instance()->throwException("Invalid number of arguments to exec, 1 required");
            }
            const Value& arg = args.front();
            if (arg.type() != Value::Type_String) {
                return ScriptEngine::instance()->throwException("Argument to exec needs to be a string");
            }
            WindowManager::SharedPtr wm = WindowManager::instance();
            Util::launch(arg.toString(), wm->displayString());
            return true;
        });

    // --------------- nwm.workspace ---------------

    auto workspace = nwm->child("workspace");

    workspace->registerProperty("count",
                                []() -> Value { return WindowManager::instance()->workspaces().size(); },
                                [](const Value &value) {
                                    const int count = value.toInteger();
                                    if (count <= 0) {
                                        return ScriptEngine::instance()->throwException<void>("Invalid workspace count");
                                    }
                                    WindowManager::instance()->setWorkspaceCount(count);
                                });


    workspace->registerFunction("moveTo", [](const List<Value>& args) -> Value {
            if (args.isEmpty())
                return Value();
            const int32_t ws = args[0].toInteger();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return ScriptEngine::instance()->throwException("Invalid workspace");
            Workspace::SharedPtr dst = wss[ws];
            Workspace::SharedPtr src = Workspace::active();
            if (dst == src)
                return Value();
            Client::SharedPtr client = src->focusedClient();
            dst->addClient(client);
            return Value();
        });

    workspace->registerFunction("select", [](const List<Value>& args) -> Value{
            if (args.isEmpty())
                return Value();
            const int32_t ws = args[0].toInteger();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return ScriptEngine::instance()->throwException("Invalid workspace");
            wss[ws]->activate();
            return Value();
        });

    // --------------- nwm.layout ---------------

    auto layout = nwm->child("layout");

    layout->registerFunction("toggleOrientation", [](const List<Value>&) -> Value {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return Value();
            const Layout::Direction dir = parent->direction();
            switch (dir) {
            case Layout::LeftRight:
                parent->setDirection(Layout::TopBottom);
                break;
            case Layout::TopBottom:
                parent->setDirection(Layout::LeftRight);
                break;
            }
            parent->dump();
            return Value();
        });

    layout->registerFunction("adjust", [](const List<Value>& args) -> Value {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return Value();
            const int adjust = args.isEmpty() ? 10 : args[0].toInteger();
            parent->adjust(adjust);
            return Value();
        });

    layout->registerFunction("adjustLeft", [](const List<Value>&) -> Value {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return Value();
            parent->adjust(-10);
            return Value();
        });

    layout->registerFunction("adjustRight", [](const List<Value>&) -> Value {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return Value();
            parent->adjust(10);
            return Value();
        });

    // --------------- nwm.kbd ---------------

    auto kbd = nwm->child("kbd");

    kbd->registerFunction("set", [](const List<Value> &args) -> Value {
            if (args.size() != 2) {

            }

        });
}

JavaScript::~JavaScript()
{
}

Value JavaScript::evaluateFile(const Path &file, String *err)
{
    if (!file.isFile()) {
        if (err)
            *err = String::format<128>("Can't open %s for reading", file.constData());
        return Value();
    }
    const String code = file.readAll();
    return evaluate(code, Path(), err);
}
