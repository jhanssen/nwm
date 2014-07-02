#include "JavaScript.h"
#include "Util.h"
#include "WindowManager.h"
#include <rct/Log.h>

JavaScript::JavaScript()
    : ScriptEngine()
{
    auto global = globalObject();
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
