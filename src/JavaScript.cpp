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
    // --------------- Client class ---------------
    mClientClass = Class::create("Client");
    mClientClass->interceptPropertyName(
        // getter, return value
        [](const Object::SharedPtr& obj, const String& prop) -> Value {
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                if (prop == "title")
                    return client->wmName();
                if (prop == "class")
                    return client->className();
                if (prop == "instance")
                    return client->instanceName();
                if (prop == "floating")
                    return client->isFloating();
                if (prop == "dialog")
                    return client->isDialog();
                if (prop == "window")
                    return static_cast<int32_t>(client->window());
                if (prop == "focused") {
                    assert(client->workspace());
                    return (client->workspace() == Workspace::active() && Workspace::active()->focusedClient() == client);
                }
            }
            return Value();
        },
        // setter return the value set
        [](const Object::SharedPtr& obj, const String& prop, const Value& value) -> Value {
            if (prop == "floating") {
                if (value.type() != Value::Type_Boolean) {
                    return instance()->throwException<Value>("Client.floating needs to be a boolean");
                }
                Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
                if (Client::SharedPtr client = weak.lock()) {
                    client->setFloating(value.toBool());
                }
            }
            return Value();
        },
        // query, return Class::QueryResult
        [](const String& prop) -> Value {
            if (prop == "title" || prop == "class" || prop == "instance" || prop == "dialog" || prop == "window" || prop == "focused")
                return Class::ReadOnly|Class::DontDelete;
            if (prop == "floating")
                return Class::DontDelete;
            return Value();
        },
        // deleter, return bool
        [](const String& prop) -> Value {
            return Value();
        },
        // enumerator, return List of property names intercepted
        []() -> Value {
            return List<Value>() << "title" << "class" << "instance" << "floating" << "dialog" << "window" << "focused";
        });
    mClientClass->registerFunction("raise", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->raise();
                WindowManager::SharedPtr wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });


    auto global = globalObject();

    // --------------- console ---------------
    auto console = global->child("console");
    console->registerFunction("log", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.isEmpty()) {
                return instance()->throwException<Value>("No arguments passed to console.log");
            }
            logValues(stdout, args);
            return Value::undefined();
       });
    console->registerFunction("error", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.isEmpty()) {
                return instance()->throwException<Value>("No arguments passed to console.log");
            }
            logValues(stderr, args);
            return Value::undefined();
        });

    // --------------- nwm ---------------
    auto nwm = global->child("nwm");
    nwm->registerFunction("launch", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.size() != 1) {
                return instance()->throwException<Value>("Invalid number of arguments to launch, 1 required");
            }
            const Value& arg = args.front();
            if (arg.type() != Value::Type_String) {
                return instance()->throwException<Value>("Argument to launch needs to be a string");
            }
            WindowManager::SharedPtr wm = WindowManager::instance();
            Util::launch(arg.toString(), wm->displayString());
            return true;
        });

    nwm->registerFunction("exec", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.size() < 1) {
                return instance()->throwException<Value>("Invalid number of arguments to exec, at least 1 required");
            }
            const Value& arg = args.front();
            if (arg.type() != Value::Type_String) {
                return instance()->throwException<Value>("First argument to exec needs to be a string");
            }
            List<String> processArgs;
            for (int i = 1; i < args.size(); ++i) {
                if (args[i].type() != Value::Type_String) {
                    return instance()->throwException<Value>("All arguments to exec needs to be a string");
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
                return instance()->throwException<Value>("exec failed for " + path);
            return proc.readAllStdOut();
        });
    nwm->registerFunction("on", [this](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.size() != 2) {
                return instance()->throwException<Value>("Invalid number of arguments to nwm.on, 2 required");
            }
            const Value& name = args.at(0);
            const Value& func = args.at(1);
            if (name.type() != Value::Type_String) {
                return instance()->throwException<Value>("First argument to nwm.on needs to be a string");
            }
            if (func.type() != Value::Type_Custom) {
                return instance()->throwException<Value>("First argument to nwm.on needs to be a function");
            }
            mOns[name.toString()] = func;
            return Value::undefined();
        });
    nwm->registerProperty("clients",
                          [this](const Object::SharedPtr&) -> Value {
                              List<Value> ret;
                              ret.reserve(mClients.size());
                              for (auto client : mClients) {
                                  ret.append(client->jsValue());
                              }
                              return ret;
                          });
    nwm->registerProperty("focusedClient",
                          [](const Object::SharedPtr&) -> Value {
                              if (Workspace::SharedPtr activeWorkspace = Workspace::active()) {
                                  auto client = activeWorkspace->focusedClient();
                                  if (client)
                                      return client->jsValue();
                              }
                              return Value::undefined();
                          });

    nwm->registerProperty("moveModifier",
                          [](const Object::SharedPtr&) -> Value {
                              return WindowManager::instance()->moveModifier();
                          },
                          [](const Object::SharedPtr&, const Value& value) {
                              if (value.type() != Value::Type_String) {
                                  return instance()->throwException<void>("Move modifier needs to be a string");
                              }
                              WindowManager::instance()->setMoveModifier(value.toString());
                              if (WindowManager::instance()->moveModifierMask() == XCB_NO_SYMBOL) {
                                  instance()->throwException<void>("Invalid move modifier");
                              }
                          });
    nwm->registerProperty("focusPolicy",
                          [](const Object::SharedPtr&) -> Value {
                              return WindowManager::instance()->focusPolicy();
                          },
                          [](const Object::SharedPtr&, const Value& value) {
                              if (value.type() != Value::Type_Integer) {
                                  return instance()->throwException<void>("Focus policy needs to be an integer");
                              }
                              const int fp = value.toInteger();
                              switch (fp) {
                              case WindowManager::FocusFollowsMouse:
                              case WindowManager::FocusClick:
                                  break;
                              default:
                                  return instance()->throwException<void>("Invalid focus policy");
                              }
                              WindowManager::instance()->setFocusPolicy(static_cast<WindowManager::FocusPolicy>(fp));
                          });
    nwm->setProperty("FocusFollowsMouse", WindowManager::FocusFollowsMouse);
    nwm->setProperty("FocusClick", WindowManager::FocusClick);

    // --------------- nwm.workspace ---------------
    auto workspace = nwm->child("workspace");
    workspace->setProperty("Stack", StackLayout::Type);
    workspace->setProperty("Grid", GridLayout::Type);
    workspace->registerFunction("add", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            unsigned int layoutType = GridLayout::Type;
            if (!args.isEmpty()) {
                if (args.size() > 1)
                    return instance()->throwException<Value>("workspace.add takes zero or one argument");
                const Value& v = args.front();
                if (v.type() != Value::Type_Map)
                    return instance()->throwException<Value>("workspace.add argument needs to be an object");
                const Value& t = v["type"];
                if (t.type() != Value::Type_Invalid) {
                    if (t.type() != Value::Type_Integer)
                        return instance()->throwException<Value>("workspace.add type needs to be an integer");
                    layoutType = t.toInteger();
                    switch (layoutType) {
                    case StackLayout::Type:
                    case GridLayout::Type:
                        break;
                    default:
                        return instance()->throwException<Value>("workspace.add invalid layout type");
                    }
                }
            }
            WindowManager::instance()->addWorkspace(layoutType);
            return Value::undefined();
        });
    workspace->registerFunction("moveTo", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.isEmpty())
                return Value::undefined();
            const int32_t ws = args[0].toInteger();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return instance()->throwException<Value>("Invalid workspace");
            Workspace::SharedPtr dst = wss[ws];
            Workspace::SharedPtr src = Workspace::active();
            if (dst == src)
                return Value::undefined();
            Client::SharedPtr client = src->focusedClient();
            dst->addClient(client);
            return Value::undefined();
        });
    workspace->registerFunction("select", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.isEmpty())
                return Value::undefined();
            const int32_t ws = args[0].toInteger();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return instance()->throwException<Value>("Invalid workspace");
            wss[ws]->activate();
            WindowManager::SharedPtr wm = WindowManager::instance();
            xcb_ewmh_set_current_desktop(wm->ewmhConnection(), wm->screenNo(), ws);
            return Value::undefined();
        });
    workspace->registerFunction("raiseLast", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            Workspace::SharedPtr active = Workspace::active();
            if (!active)
                return Value::undefined();
            active->raise(Workspace::Last);
            return Value::undefined();
        });

    // --------------- nwm.layout ---------------
    auto layout = nwm->child("layout");
    layout->registerFunction("toggleOrientation", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value::undefined();
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
            return Value::undefined();
        });
    layout->registerFunction("adjust", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value::undefined();
            const int adjust = args.isEmpty() ? 10 : args[0].toInteger();
            parent->adjust(adjust);
            return Value::undefined();
        });
    layout->registerFunction("adjustLeft", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value::undefined();
            parent->adjust(-10);
            return Value::undefined();
        });
    layout->registerFunction("adjustRight", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            GridLayout::SharedPtr parent = gridParent();
            if (!parent)
                return Value::undefined();
            parent->adjust(10);
            return Value::undefined();
        });

    // --------------- nwm.kbd ---------------
    auto kbd = nwm->child("kbd");
    kbd->registerFunction("set", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            if (args.size() != 2)
                return instance()->throwException<Value>("Invalid number of arguments to kbd.set, 2 required");
            const Value& key = args.at(0);
            const Value& func = args.at(1);
            if (key.type() != Value::Type_String)
                return instance()->throwException<Value>("Invalid first argument to kbd.set, needs to be a string");
            if (func.type() != Value::Type_Custom)
                return instance()->throwException<Value>("Invalid second argument to kbd.set, needs to be a JS function");
            Keybinding binding(key.toString(), func);
            if (!binding.isValid())
                return instance()->throwException<Value>(String::format<64>("Couldn't parse keybind for %s",
                                                                            key.toString().constData()));
            WindowManager::instance()->bindings().add(binding);
            return Value::undefined();
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

void JavaScript::onClient(const Client::SharedPtr& client)
{
    mClients.append(client);
    auto it = mOns.find("client");
    if (it == mOns.end())
        return;
    Object::SharedPtr func = toObject(it->second);
    if (!func || !func->isFunction()) {
        error() << "onClient is not a function";
        return;
    }
    func->call({ client->jsValue() });
}

void JavaScript::onClientDestroyed(const Client::SharedPtr &client)
{
    const int idx = mClients.indexOf(client);
    assert(idx != -1);
    mClients.removeAt(idx);
    auto it = mOns.find("clientRemoved");
    if (it == mOns.end())
        return;
    Object::SharedPtr func = toObject(it->second);
    if (!func || !func->isFunction()) {
        error() << "clientRemoved is not a function";
        return;
    }
    func->call({ client->jsValue() });
}

void JavaScript::reload()
{
    for (const auto &client : mClients) {
        mClients->clearScriptValue();
    }
    mClients.clear();
    mOns.clear();
    mClientClass.reset();
    init();
}
