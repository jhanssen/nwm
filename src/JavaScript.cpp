#include "JavaScript.h"
#include "GridLayout.h"
#include "StackLayout.h"
#include "Util.h"
#include "Keybinding.h"
#include "WindowManager.h"
#include <rct/Log.h>
#include <rct/Process.h>

JavaScript::JavaScript()
    : ScriptEngine()
{
}

static inline GridLayout::SharedPtr gridParent()
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (!wm)
        return GridLayout::SharedPtr();
    Client::SharedPtr current = Workspace::active()->focusedClient();
    if (!current)
        return GridLayout::SharedPtr();
    const Layout::SharedPtr& layout = current->layout();
    if (!layout || layout->type() != GridLayout::Type)
        return GridLayout::SharedPtr();
    const GridLayout::SharedPtr& parent = std::static_pointer_cast<GridLayout>(layout)->parent();
    if (!parent)
        return GridLayout::SharedPtr();
    return parent;
}

static inline void logValues(FILE* file, const List<Value>& args)
{
    String str;
    {
        Log log(&str);
        for (const Value& arg : args) {
            const String& str = arg.toString();
            if (str.isEmpty())
                log << arg;
            else
                log << str;
        }
    }
    fprintf(file, "%s\n", str.constData());
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
            logValues(stdout, args);
            return Value();
       });
    console->registerFunction("error", [](const List<Value>& args) -> Value {
            if (args.isEmpty()) {
                return ScriptEngine::instance()->throwException("No arguments passed to console.log");
            }
            logValues(stderr, args);
            return Value();
        });

    // --------------- nwm ---------------
    auto nwm = global->child("nwm");
    nwm->registerFunction("launch", [](const List<Value>& args) -> Value {
            if (args.size() != 1) {
                return ScriptEngine::instance()->throwException("Invalid number of arguments to launch, 1 required");
            }
            const Value& arg = args.front();
            if (arg.type() != Value::Type_String) {
                return ScriptEngine::instance()->throwException("Argument to launch needs to be a string");
            }
            WindowManager::SharedPtr wm = WindowManager::instance();
            Util::launch(arg.toString(), wm->displayString());
            return true;
        });
    nwm->registerFunction("exec", [](const List<Value>& args) -> Value {
            if (args.size() < 1) {
                return ScriptEngine::instance()->throwException("Invalid number of arguments to exec, at least 1 required");
            }
            const Value& arg = args.front();
            if (arg.type() != Value::Type_String) {
                return ScriptEngine::instance()->throwException("First argument to exec needs to be a string");
            }
            List<String> processArgs;
            for (int i = 1; i < args.size(); ++i) {
                if (args[i].type() != Value::Type_String) {
                    return ScriptEngine::instance()->throwException("All arguments to exec needs to be a string");
                }
                processArgs.append(args[i].toString());
            }
            List<String> environ = Process::environment();
            // remove "DISPLAY" if it's present
            {
                auto it = environ.begin();
                const auto end = environ.cend();
                while (it != end) {
                    if (it->startsWith("DISPLAY=")) {
                        environ.erase(it);
                        break;
                    }
                    ++it;
                }
            }
            // reinsert "DISPLAY"
            environ.append("DISPLAY=" + WindowManager::instance()->displayString());

            const Path path = arg.toString();
            Process proc;
            if (proc.exec(path, processArgs, environ) != Process::Done)
                return ScriptEngine::instance()->throwException("exec failed for " + path);
            return proc.readAllStdOut();
        });
    nwm->registerProperty("moveModifier",
                          []() -> Value {
                              return WindowManager::instance()->moveModifier();
                          },
                          [](const Value& value) {
                              if (value.type() != Value::Type_String) {
                                  return ScriptEngine::instance()->throwException<void>("Move modifier needs to be a string");
                              }
                              WindowManager::instance()->setMoveModifier(value.toString());
                              if (WindowManager::instance()->moveModifierMask() == XCB_NO_SYMBOL) {
                                  ScriptEngine::instance()->throwException<void>("Invalid move modifier");
                              }
                          });
    nwm->registerProperty("focusPolicy",
                          []() -> Value {
                              return WindowManager::instance()->focusPolicy();
                          },
                          [](const Value& value) {
                              if (value.type() != Value::Type_Integer) {
                                  return ScriptEngine::instance()->throwException<void>("Focus policy needs to be an integer");
                              }
                              const int fp = value.toInteger();
                              switch (fp) {
                              case WindowManager::FocusFollowsMouse:
                              case WindowManager::FocusClick:
                                  break;
                              default:
                                  return ScriptEngine::instance()->throwException<void>("Invalid focus policy");
                              }
                              WindowManager::instance()->setFocusPolicy(static_cast<WindowManager::FocusPolicy>(fp));
                          });
    nwm->setProperty("FocusFollowsMouse", WindowManager::FocusFollowsMouse);
    nwm->setProperty("FocusClick", WindowManager::FocusClick);

    // --------------- nwm.workspace ---------------
    auto workspace = nwm->child("workspace");
    workspace->setProperty("Stack", StackLayout::Type);
    workspace->setProperty("Grid", GridLayout::Type);
    workspace->registerFunction("add", [](const List<Value>& args) -> Value {
            unsigned int layoutType = GridLayout::Type;
            if (!args.isEmpty()) {
                if (args.size() > 1)
                    return ScriptEngine::instance()->throwException("workspace.add takes zero or one argument");
                const Value& v = args.front();
                if (v.type() != Value::Type_Map)
                    return ScriptEngine::instance()->throwException("workspace.add argument needs to be an object");
                const Value& t = v["type"];
                if (t.type() != Value::Type_Invalid) {
                    if (t.type() != Value::Type_Integer)
                        return ScriptEngine::instance()->throwException("workspace.add type needs to be an integer");
                    layoutType = t.toInteger();
                    switch (layoutType) {
                    case StackLayout::Type:
                    case GridLayout::Type:
                        break;
                    default:
                        return ScriptEngine::instance()->throwException("workspace.add invalid layout type");
                    }
                }
            }
            WindowManager::instance()->addWorkspace(layoutType);
            return Value();
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
    workspace->registerFunction("select", [](const List<Value>& args) -> Value {
            if (args.isEmpty())
                return Value();
            const int32_t ws = args[0].toInteger();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return ScriptEngine::instance()->throwException("Invalid workspace");
            wss[ws]->activate();
            WindowManager::SharedPtr wm = WindowManager::instance();
            xcb_ewmh_set_current_desktop(wm->ewmhConnection(), wm->screenNo(), ws);
            return Value();
        });
    workspace->registerFunction("raiseLast", [](const List<Value>&) -> Value {
            Workspace::SharedPtr active = Workspace::active();
            if (!active)
                return Value();
            active->raise(Workspace::Last);
            return Value();
        });

    // --------------- nwm.layout ---------------
    auto layout = nwm->child("layout");
    layout->registerFunction("toggleOrientation", [](const List<Value>&) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value();
            const GridLayout::Direction dir = parent->direction();
            switch (dir) {
            case GridLayout::LeftRight:
                parent->setDirection(GridLayout::TopBottom);
                break;
            case GridLayout::TopBottom:
                parent->setDirection(GridLayout::LeftRight);
                break;
            }
            parent->dump();
            return Value();
        });
    layout->registerFunction("adjust", [](const List<Value>& args) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value();
            const int adjust = args.isEmpty() ? 10 : args[0].toInteger();
            parent->adjust(adjust);
            return Value();
        });
    layout->registerFunction("adjustLeft", [](const List<Value>&) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value();
            parent->adjust(-10);
            return Value();
        });
    layout->registerFunction("adjustRight", [](const List<Value>&) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value();
            parent->adjust(10);
            return Value();
        });

    // --------------- nwm.kbd ---------------
    auto kbd = nwm->child("kbd");
    kbd->registerFunction("set", [](const List<Value> &args) -> Value {
            if (args.size() != 2)
                return ScriptEngine::instance()->throwException("Invalid number of arguments to kbd.set, 2 required");
            const Value& key = args.at(0);
            const Value& func = args.at(1);
            if (key.type() != Value::Type_String)
                return ScriptEngine::instance()->throwException("Invalid first argument to kbd.set, needs to be a string");
            if (func.type() != Value::Type_Custom)
                return ScriptEngine::instance()->throwException("Invalid second argument to kbd.set, needs to be a JS function");
            Keybinding binding(key.toString(), func);
            if (!binding.isValid())
                return ScriptEngine::instance()->throwException(String::format<64>("Couldn't parse keybind for %s",
                                                                                   key.toString().constData()));
            WindowManager::instance()->bindings().add(binding);
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
