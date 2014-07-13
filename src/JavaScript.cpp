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

static inline GridLayout *gridParent()
{
    WindowManager *wm = WindowManager::instance();
    if (!wm)
        return 0;
    Client::SharedPtr current = wm->focusedClient();
    if (!current)
        return 0;
    Layout *layout = current->layout();
    if (!layout || layout->type() != GridLayout::Type)
        return 0;
    GridLayout *parent = static_cast<GridLayout*>(layout)->parent();
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

bool JavaScript::init(String *err)
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
                    return Value(WindowManager::instance()->focusedClient() == client);
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
    mClientClass->registerFunction("activate", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->raise();
                client->focus();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("raise", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->raise();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("focus", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->focus();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("close", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->close();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("kill", [](const Object::SharedPtr &obj, const List<Value> &args) -> Value {
            if (args.size() != 1) {
                return instance()->throwException<Value>("Invalid number of arguments to Client.kill, 1 required");
            }
            if (!args[0].isInteger()) {
                return instance()->throwException<Value>("Invalid arguments to Client.kill. First arg must be an integer");
            }
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->kill(args[0].toInteger());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("move", [](const Object::SharedPtr &obj, const List<Value> &args) -> Value {
            if (args.size() != 2) {
                return instance()->throwException<Value>("Invalid number of arguments to Client.move, 2 required");
            }
            if (!args[0].isInteger() || !args[1].isInteger()) {
                return instance()->throwException<Value>("Invalid arguments to Client.move. Args must be integers");
            }
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->move(Point({ static_cast<uint32_t>(args[0].toInteger()),
                                     static_cast<uint32_t>(args[1].toInteger()) }));
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("resize", [](const Object::SharedPtr &obj, const List<Value> &args) -> Value {
            if (args.size() != 2) {
                return instance()->throwException<Value>("Invalid number of arguments to Client.resize, 2 required");
            }
            if (!args[0].isInteger() || !args[1].isInteger()) {
                return instance()->throwException<Value>("Invalid arguments to Client.resize. Args must be integers");
            }
            Client::WeakPtr weak = obj->extraData<Client::WeakPtr>();
            if (Client::SharedPtr client = weak.lock()) {
                client->resize(Size({ static_cast<uint32_t>(args[0].toInteger()),
                                      static_cast<uint32_t>(args[1].toInteger()) }));
                WindowManager *wm = WindowManager::instance();
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
            WindowManager *wm = WindowManager::instance();
            Util::launch(arg.toString(), wm->displayString());
            return true;
        });
    nwm->registerFunction("readFile", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.size() != 1) {
                return instance()->throwException<Value>("Invalid number of arguments to readFile, 1 required");
            }
            const Value &arg = args.front();
            if (!arg.isString()) {
                return instance()->throwException<Value>("Invalid arguments to readFile. First arg must be a string");
            }
            const Path path = arg.toString();
            if (!path.isFile())
                return Value::undefined();
            return path.readAll();
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
    nwm->registerFunction("restart", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            WindowManager::instance()->restart();
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
                              auto client = WindowManager::instance()->focusedClient();
                              if (client)
                                  return client->jsValue();
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
            int screenNumber = WindowManager::AllScreens;
            if (!args.isEmpty()) {
                if (args.size() > 1)
                    return instance()->throwException<Value>("workspace.add takes zero or one argument");
                const Value& v = args.front();
                if (v.type() != Value::Type_Map)
                    return instance()->throwException<Value>("workspace.add argument needs to be an object");

                if (v.contains("type")) {
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

                if (v.contains("screen")) {
                    const Value& s = v["screen"];
                    if (s.type() != Value::Type_Invalid) {
                        if (s.type() != Value::Type_Integer)
                            return instance()->throwException<Value>("workspace.add screen needs to be an integer");
                        screenNumber = s.toInteger();
                        if (screenNumber < 0 || screenNumber >= WindowManager::instance()->screenCount()) {
                            return instance()->throwException<Value>("workspace.add invalid screen number");
                        }
                    }
                }
            }
            WindowManager::instance()->addWorkspace(layoutType, screenNumber);
            return Value::undefined();
        });
    workspace->registerFunction("moveTo", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.isEmpty())
                return Value::undefined();
            Client::SharedPtr client = WindowManager::instance()->focusedClient();
            if (!client)
                return Value::undefined();
            const int32_t ws = args[0].toInteger();
            const List<Workspace*> &wss = WindowManager::instance()->workspaces(client->screenNumber());
            if (ws < 0 || ws >= wss.size())
                return instance()->throwException<Value>("Invalid workspace");
            Workspace *dst = wss[ws];
            Workspace *src = WindowManager::instance()->activeWorkspace(client->screenNumber());
            if (dst == src)
                return Value::undefined();
            dst->addClient(client);
            return Value::undefined();
        });
    workspace->registerFunction("select", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.isEmpty())
                return Value::undefined();
            const int32_t ws = args[0].toInteger();
            int screenNumber = 0;
            WindowManager* wm = WindowManager::instance();
            if (wm->screenCount() > 1) {
                if (args.size() == 1) {
                    screenNumber = wm->currentScreen();
                } else {
                    screenNumber = args[1].toInteger();
                }
            }
            const List<Workspace*> &wss = WindowManager::instance()->workspaces(screenNumber);
            if (ws < 0 || ws >= wss.size())
                return instance()->throwException<Value>("Invalid workspace");
            wss[ws]->activate();
            xcb_ewmh_set_current_desktop(wm->ewmhConnection(), wss[ws]->screenNumber(), ws);
            return Value::undefined();
        });
    workspace->registerFunction("raiseLast", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            Client::SharedPtr focusedClient = WindowManager::instance()->focusedClient();
            if (!focusedClient)
                return Value::undefined();
            Workspace *active = WindowManager::instance()->activeWorkspace(focusedClient->screenNumber());
            if (!active)
                return Value::undefined();
            active->raise(Workspace::Last);
            return Value::undefined();
        });

    // --------------- nwm.layout ---------------
    auto layout = nwm->child("layout");
    layout->registerFunction("toggleOrientation", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            GridLayout *parent = gridParent();
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
            GridLayout *parent = gridParent();
            if (!parent)
                return Value::undefined();
            const int adjust = args.isEmpty() ? 10 : args[0].toInteger();
            parent->adjust(adjust);
            return Value::undefined();
        });
    layout->registerFunction("adjustLeft", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            GridLayout *parent = gridParent();
            if (!parent)
                return Value::undefined();
            parent->adjust(-10);
            return Value::undefined();
        });
    layout->registerFunction("adjustRight", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            GridLayout *parent = gridParent();
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

    for (int i=mConfigFiles.size() - 1; i>=0; --i) {
        const String contents = mConfigFiles[i].readAll();
        if (!contents.isEmpty()) {
            String e;
            evaluate(contents, mConfigFiles[i], &e);
            if (!e.isEmpty()) {
                if (err)
                    *err = e;
                return false;
            }
        }
    }
    return true;
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
    onClientEvent(client, "client");
}

void JavaScript::onClientEvent(const Client::SharedPtr &client, const String &event)
{
    assert(mClients.contains(client));
    auto it = mOns.find(event);
    if (it == mOns.end()) {
        return;
    }
    Object::SharedPtr func = toObject(it->second);
    if (!func || !func->isFunction()) {
        error() << event << "is not a function";
        return;
    }
    func->call({ client->jsValue() });
}

bool JavaScript::reload(String *err)
{
    for (const auto &client : mClients) {
        client->clearJSValue();
    }
    mClients.clear();
    mOns.clear();
    mClientClass.reset();
    return init(err);
}
