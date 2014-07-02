#include "JavaScript.h"
#include "Util.h"
#include "WindowManager.h"
#include <rct/Log.h>

JavaScript::JavaScript()
{
    setup();
}

JavaScript::~JavaScript()
{
}

void JavaScript::setup()
{
#ifdef HAVE_SCRIPTENGINE
    auto global = engine.globalObject();
    auto nwm = global->child("nwm");
    nwm->registerFunction("exec", [](const List<Value>& args) -> Value {
            if (args.size() != 1) {
                ScriptEngine::instance()->throwException("Invalid number of arguments to exec, 1 required");
                return Value();
            }
            const Value& arg = args.front();
            if (arg.type() != Value::Type_String) {
                ScriptEngine::instance()->throwException("Argument to exec needs to be a string");
                return Value();
            }
            WindowManager::SharedPtr wm = WindowManager::instance();
            Util::launch(arg.toString(), wm->displayString());
            return true;
        });
    auto console = global->child("console");
    console->registerFunction("log", [](const List<Value>& args) -> Value {
            if (args.isEmpty()) {
                ScriptEngine::instance()->throwException("No arguments passed to console.log");
                return Value();
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
#endif
}

void JavaScript::execute(const String& code)
{
#ifdef HAVE_SCRIPTENGINE
    String err;
    engine.evaluate(code, Path(), &err);
    if (!err.isEmpty()) {
        error() << err;
    }
#endif
}
