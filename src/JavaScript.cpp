#include "JavaScript.h"
#include "GridLayout.h"
#include "StackLayout.h"
#include "Util.h"
#include "Keybinding.h"
#include "WindowManager.h"
#include <rct/Log.h>
#include <rct/Process.h>

enum ReadValueFlags {
    None = 0x0,
    NotRequired = 0x1,
    UndefinedValue = 0x2
};

template <typename T> static T convertValue(const Value &value, bool &ok) { return value.convert<T>(&ok); }
template <typename T> static T readValue(const Value &val, bool &ok, unsigned int flags = 0, const T &defaultValue = T())
{
    if (val.isInvalid() || (!(flags & UndefinedValue) && val.isUndefined())) {
        ok = flags & NotRequired;
        return defaultValue;
    }

    const T ret = convertValue<T>(val, ok);
    if (!ok)
        return defaultValue;
    return ret;
}

template <typename T>
static T readChild(const Value &map, const String &name, bool &ok, unsigned int flags = 0, const T &defaultValue = T())
{
    if (!map.isMap() || !map.contains(name)) {
        ok = flags & NotRequired;
        return defaultValue;
    }
    return readValue<T>(map[name], ok, flags, defaultValue);
}

template <typename T>
static T readChild(const Value &array, int idx, bool &ok, unsigned int flags = 0, const T &defaultValue = T())
{
    assert(idx >= 0);
    if (!array.isList() || array.count() <= idx) {
        ok = flags & NotRequired;
        return defaultValue;
    }
    return readValue<T>(array[idx], ok, flags, defaultValue);
}

template <> inline Color convertValue<Color>(const Value &value, bool &ok)
{
    if (value.isUndefined()) {
        ok = true;
        return Color();
    } else if (!value.isMap()) {
        ok = false;
        return Color();
    }

    const int r = readChild<int>(value, "r", ok, NotRequired, 0);
    if (!ok || r < 0 || r > 255)
        return Color();

    const int g = readChild<int>(value, "g", ok, NotRequired, 0);
    if (!ok || g < 0 || g > 255)
        return Color();

    const int b = readChild<int>(value, "b", ok, NotRequired, 0);
    if (!ok || b < 0 || b > 255)
        return Color();

    const int a = readChild<int>(value, "a", ok, NotRequired, 255);
    if (!ok || a < 0 || a > 255)
        return Color();

    Color ret;
    ret.r = static_cast<uint8_t>(r);
    ret.g = static_cast<uint8_t>(g);
    ret.b = static_cast<uint8_t>(b);
    ret.a = static_cast<uint8_t>(a);
    return ret;
}

template <> inline Rect convertValue<Rect>(const Value &value, bool &ok)
{
    if (value.isUndefined()) {
        ok = true;
        return Rect();
    } else if (!value.isMap()) {
        ok = false;
        return Rect();
    }
    Rect rect;
    rect.width = readChild<int>(value, "width", ok);
    if (!ok || !rect.width)
        return Rect();

    rect.height = readChild<int>(value, "height", ok);
    if (!ok || !rect.height)
        return Rect();

    rect.x = readChild<int>(value, "x", ok, NotRequired, 0);
    if (!ok)
        return Rect();

    rect.y = readChild<int>(value, "y", ok, NotRequired, 0);
    if (!ok)
        return Rect();

    return rect;
}

template <> inline Font convertValue<Font>(const Value &value, bool &ok)
{
    if (!value.isMap()) {
        ok = false;
        return Font();
    }

    const String family = readChild<String>(value, "family", ok);
    if (!ok)
        return Font();

    const int pointSize = readChild<int>(value, "pointSize", ok);
    if (!ok)
        return Font();

    Font font;
    font.setFamily(family);
    font.setPointSize(pointSize);
    return font;
}

JavaScript::JavaScript()
    : ScriptEngine()
{
}

static inline GridLayout *gridParent()
{
    WindowManager *wm = WindowManager::instance();
    if (!wm)
        return 0;
    Client *current = wm->focusedClient();
    if (!current)
        return 0;
    Layout *layout = current->layout();
    if (!layout || layout->type() != GridLayout::Type)
        return 0;
    GridLayout *parent = static_cast<GridLayout*>(layout)->parent();
    return parent;
}

static inline Value logValues(FILE* file, const List<Value> &args)
{
    if (args.isEmpty())
        return ScriptEngine::instance()->throwException<Value>("No arguments passed to console function");

    String str;
    {
        Log log(&str);
        for (const Value &arg : args) {
            const String &str = arg.toString();
            if (str.isEmpty()) {
                log << arg;
            } else {
                log << str;
            }
        }
    }
    fprintf(file, "%s\n", str.constData());
    return Value::undefined();
}

static inline Value fromRect(const Rect &rect)
{
    Value ret;
    ret["x"] = rect.x;
    ret["y"] = rect.y;
    ret["width"] = rect.width;
    ret["height"] = rect.height;
    return ret;
}

static inline Value fromSize(const Size &size)
{
    Value ret;
    ret["width"] = size.width;
    ret["height"] = size.height;
    return ret;
}

bool JavaScript::init(String *err)
{
    // --------------- Client class ---------------
    mClientClass = Class::create("Client");
    mClientClass->interceptPropertyName(
        // getter, return value
        [](const Object::SharedPtr &obj, const String &prop) -> Value {
            if (Client *client = obj->extraData<Client*>()) {
                if (prop == "title")
                    return client->wmName();
                if (prop == "class")
                    return client->className();
                if (prop == "instance")
                    return client->instanceName();
                if (prop == "floating")
                    return client->isFloating();
                if (prop == "screen")
                    return client->screenNumber();
                if (prop == "dialog")
                    return client->isDialog();
                if (prop == "window")
                    return static_cast<int32_t>(client->window());
                if (prop == "rect")
                    return fromRect(client->rect());
                if (prop == "focused")
                    return Value(WindowManager::instance()->focusedClient() == client);
            }
            return Value();
        },
        // setter return the value set
        [](const Object::SharedPtr &obj, const String &prop, const Value &value) -> Value {
            bool ok;
            if (prop == "floating") {
                const bool floating = readValue<bool>(value, ok);
                if (!ok)
                    return instance()->throwException<Value>("Client.floating needs to be a boolean");
                if (Client *client = obj->extraData<Client*>()) {
                    client->setFloating(floating);
                }
            } else if (prop == "backgroundColor") {
                const Color color = readValue<Color>(value, ok, UndefinedValue);
                if (!ok)
                    return instance()->throwException<Value>("Client.backgroundColor needs to be a color or undefined");
                if (Client *client = obj->extraData<Client*>()) {
                    client->setBackgroundColor(color);
                }
            } else if (prop == "text") {
                if (value.isUndefined() || value.isInvalid()) {
                    if (Client *client = obj->extraData<Client*>())
                        client->clearText();
                    return Value::undefined();
                } else if (!value.isMap()) {
                    return instance()->throwException<Value>("Client.text needs to be an object");
                }

                const Rect rect = readChild<Rect>(value, "rect", ok, NotRequired);
                if (!ok)
                    return instance()->throwException<Value>("Client.rect needs to be an object");

                const String text = readChild<String>(value, "text", ok);
                if (!ok)
                    return instance()->throwException<Value>("Client.text needs to have a text property");

                const Color color = readChild<Color>(value, "color", ok, NotRequired);
                if (!ok)
                    return instance()->throwException<Value>("Client.text color needs to be a color");

                const Font font = readChild<Font>(value, "font", ok);
                if (!ok || font.family().isEmpty())
                    return instance()->throwException<Value>("Client.text needs to have a font property");

                if (Client *client = obj->extraData<Client*>()) {
                    if (!client->isOwned())
                        return instance()->throwException<Value>("Client.text can only be used for nwm-created clients");
                    client->setText(rect, font, color, text);
                }
            }
            return Value::undefined();
        },
        // query, return Class::QueryResult
        [](const String &prop) -> Value {
            if (prop == "title" || prop == "class" || prop == "instance" ||
                prop == "dialog" || prop == "window" || prop == "focused" ||
                prop == "screen" || prop == "rect") {
                return Class::ReadOnly|Class::DontDelete;
            }

            if (prop == "floating" || prop == "backgroundColor" || prop == "text") {
                return Class::DontDelete;
            }
            return Value();
        },
        // deleter, return bool
        [](const String &prop) -> Value {
            return Value();
        },
        // enumerator, return List of property names intercepted
        []() -> Value {
            return List<Value>() << "title" << "class" << "instance" << "floating" << "dialog"
                                 << "window" << "focused" << "backgroundColor" << "text"
                                 << "screen" << "rect";
        });

    mClientClass->registerConstructor([](const List<Value> &args) -> Value {
            if (args.size() != 1)
                return instance()->throwException<Value>("Client constructor needs an object argument");

            bool ok;
            const Rect rect = readChild<Rect>(args, 0, ok);
            if (!ok)
                return instance()->throwException<Value>("Client constructor needs an object argument with rect");

            const String clazz = readChild<String>(args.first(), "class", ok, NotRequired);
            if (!ok)
                return instance()->throwException<Value>("Client constructor class needs to be a string");

            const String inst = readChild<String>(args.first(), "instance", ok, NotRequired);
            if (!ok)
                return instance()->throwException<Value>("Client constructor instance needs to be a string");

            WindowManager* wm = WindowManager::instance();
            Client *client = Client::create(rect, wm->currentScreen(), clazz, inst);
            return client->jsValue();
        });
    mClientClass->registerStaticFunction("fontMetrics", [](const List<Value> &args) -> Value {
            if (args.size() != 1) {
                return instance()->throwException<Value>("Client fontMetrics needs a single object argument");
            }
            bool ok;
            const int width = readChild<int>(args.first(), "width", ok, NotRequired, -1);
            if (!ok)
                return instance()->throwException<Value>("width needs to be an integer");

            const String text = readChild<String>(args.first(), "text", ok);
            if (!ok)
                return instance()->throwException<Value>("text needs to have a text property");

            const Font font = readChild<Font>(args.first(), "font", ok);
            if (!ok || font.family().isEmpty())
                return instance()->throwException<Value>("fontMetrics needs to have a font property");

            return fromSize(Graphics::fontMetrics(font, text, width));
        });
    mClientClass->registerFunction("activate", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            if (Client *client = obj->extraData<Client*>()) {
                client->raise();
                client->focus();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("raise", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            if (Client *client = obj->extraData<Client*>()) {
                client->raise();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("focus", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            if (Client *client = obj->extraData<Client*>()) {
                client->focus();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("close", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
            if (Client *client = obj->extraData<Client*>()) {
                client->close();
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("kill", [](const Object::SharedPtr &obj, const List<Value> &args) -> Value {
            bool ok;
            const int pid = readChild<int>(args, 0, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to Client.kill. First arg must be an integer");

            if (Client *client = obj->extraData<Client*>())
                client->kill(pid);
            return Value::undefined();
        });
    mClientClass->registerFunction("move", [](const Object::SharedPtr &obj, const List<Value> &args) -> Value {
            bool ok;
            const int x = readChild<int>(args, 0, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to Client.move, 2 integers required");
            const int y = readChild<int>(args, 1, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to Client.move, 2 integers required");
            if (Client *client = obj->extraData<Client*>()) {
                client->move(Point(x, y));
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });
    mClientClass->registerFunction("resize", [](const Object::SharedPtr &obj, const List<Value> &args) -> Value {
            bool ok;
            const int w = readChild<int>(args, 0, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to Client.resize, 2 integers required");
            const int h = readChild<int>(args, 1, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to Client.resize, 2 integers required");

            if (Client *client = obj->extraData<Client*>()) {
                client->resize(Size(w, h));
                WindowManager *wm = WindowManager::instance();
                assert(wm);
                xcb_flush(wm->connection());
            }
            return Value::undefined();
        });

    auto global = globalObject();

    // --------------- timers ---------------
    global->registerFunction("setTimeout", [this](const Object::SharedPtr &, const List<Value> &args) -> Value {
            return startTimer(args, Timer::SingleShot);
        });

    global->registerFunction("setInterval", [this](const Object::SharedPtr &, const List<Value> &args) -> Value {
            return startTimer(args, 0);
        });
    global->registerFunction("clearTimeout", [this](const Object::SharedPtr &, const List<Value> &args) -> Value {
            return clearTimer(args);
        });
    global->registerFunction("clearInterval", [this](const Object::SharedPtr &, const List<Value> &args) -> Value {
            return clearTimer(args);
        });
    // yes you can clear a timeout with clearInterval and vice versa

    // --------------- console ---------------
    auto console = global->child("console");
    console->registerFunction("log", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            return logValues(stdout, args);
        });
    console->registerFunction("error", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            return logValues(stderr, args);
        });

    // --------------- nwm ---------------
    auto nwm = global->child("nwm");
    nwm->registerFunction("launch", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            bool ok;
            const String command = readChild<String>(args, 0, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to launch");

            const Map<String, Value> environment = readChild<Map<String, Value> >(args, 1, ok, NotRequired);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to launch");

            Hash<String, String> env;
            for (const auto &it : environment) {
                env[it.first] = it.second.toString();
            }

            if (!env.contains("DISPLAY"))
                env["DISPLAY"] = WindowManager::instance()->displayString();

            Util::launch(command, env);
            return true;
        });
    nwm->registerFunction("readFile", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            bool ok;
            const Path path = readChild<String>(args, 0, ok);
            if (!ok)
                return instance()->throwException<Value>("Invalid arguments to readFile");
            if (!path.isFile())
                return Value::undefined();
            return path.readAll();
        });
    nwm->registerFunction("exec", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            if (args.size() < 1) {
                return instance()->throwException<Value>("Invalid number of arguments to exec, at least 1 required");
            }
            const Value &arg = args.front();
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
    nwm->registerFunction("on", [this](const Object::SharedPtr&, const List<Value> &args) -> Value {
            if (args.size() != 2) {
                return instance()->throwException<Value>("Invalid number of arguments to nwm.on, 2 required");
            }
            const Value &name = args.at(0);
            const Value &func = args.at(1);
            if (name.type() != Value::Type_String) {
                return instance()->throwException<Value>("First argument to nwm.on needs to be a string");
            }
            if (!isFunction(func)) {
                return instance()->throwException<Value>("First argument to nwm.on needs to be a function");
            }
            mOns[name.toString()] = func;
            return Value::undefined();
        });
    nwm->registerFunction("restart", [](const Object::SharedPtr&, const List<Value>&) -> Value {
            WindowManager::instance()->restart();
            return Value::undefined();
        });
    nwm->registerFunction("quit", [](const Object::SharedPtr&, const List<Value>& args) -> Value {
            if (args.size() > 1 || (args.size() == 1 && !args.first().isInteger()))
                return instance()->throwException<Value>("Invalid arguments to nwm.quit(). Needs to 0 or 1 int argument");

            WindowManager::instance()->quit(args.isEmpty() ? 0 : args.first().toInteger());
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
    nwm->registerProperty("currentScreen",
                          [](const Object::SharedPtr&) -> Value {
                              WindowManager *wm = WindowManager::instance();
                              return wm->currentScreen();
                          });
    nwm->registerProperty("preferredScreen",
                          [](const Object::SharedPtr&) -> Value {
                              WindowManager *wm = WindowManager::instance();
                              return wm->preferredScreen();
                          });
    nwm->registerProperty("screens",
                          [](const Object::SharedPtr&) -> Value {
                              WindowManager *wm = WindowManager::instance();
                              List<Value> ret;
                              const int count = wm->screenCount();
                              ret.reserve(count);
                              for (int i=0; i<count; ++i) {
                                  ret.append(fromRect(wm->rect(i)));
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
                          [](const Object::SharedPtr&, const Value &value) {
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
                          [](const Object::SharedPtr&, const Value &value) {
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
    nwm->registerProperty("pointer",
                          [](const Object::SharedPtr&) -> Value {
                              bool ok;
                              WindowManager *wm = WindowManager::instance();
                              int screen;
                              auto pointer = wm->pointer(&screen, &ok);
                              if (!ok)
                                  return instance()->throwException<Value>("Can't query pointer");
                              Value value;
                              value["x"] = static_cast<int>(pointer.x);
                              value["y"] = static_cast<int>(pointer.y);
                              value["screen"] = screen;
                              return value;
                          },
                          [](const Object::SharedPtr&, const Value &value) {
                              if (value.type() != Value::Type_Map || !value.contains("x") || !value.contains("y")) {
                                  return instance()->throwException<void>("Warp object needs to be an object with integral x, y and optionally screen and relative");
                              }
                              const Value &x = value["x"];
                              const Value &y = value["y"];
                              if (x.type() != Value::Type_Integer || y.type() != Value::Type_Integer) {
                                  return instance()->throwException<void>("Warp object needs to be an object with integral x, y and optionally screen and relative");
                              }
                              int screen = -1;
                              if (value.contains("screen")) {
                                  const Value &scr = value["screen"];
                                  if (scr.type() != Value::Type_Integer)
                                      return instance()->throwException<void>("Warp object needs to be an object with integral x, y and optionally screen and relative");
                                  screen = scr.toInteger();
                              }

                              WindowManager::PointerMode mode = WindowManager::Warp_Absolute;
                              if (value.contains("relative")) {
                                  const Value &rel = value["relative"];
                                  if (rel.type() != Value::Type_Boolean)
                                      return instance()->throwException<void>("Warp object needs to be an object with integral x, y and optionally screen and relative");
                                  if (rel.toBool()) {
                                      mode = WindowManager::Warp_Relative;
                                  }
                              }

                              WindowManager *wm = WindowManager::instance();
                              if (!wm->warpPointer(Point(x.toInteger(), y.toInteger()), screen, mode)) {
                                  instance()->throwException<void>("Invalid warp parameters");
                              }
                          });

    Value env;
    for (int i=0; environ[i]; ++i) {
        char *eq = strchr(environ[i], '=');
        if (eq) {
            env[String(environ[i], eq - environ[i])] = eq + 1;
        } else {
            env[environ[i]] = "";
        }
    }
    nwm->setProperty("env", env);
    nwm->setProperty("FocusFollowsMouse", WindowManager::FocusFollowsMouse);
    nwm->setProperty("FocusClick", WindowManager::FocusClick);

    // --------------- nwm.workspace ---------------
    auto workspace = nwm->child("workspace");
    workspace->setProperty("Stack", StackLayout::Type);
    workspace->setProperty("Grid", GridLayout::Type);
    workspace->registerFunction("add", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            unsigned int layoutType = GridLayout::Type;
            int screenNumber = WindowManager::AllScreens;
            if (!args.isEmpty()) {
                if (args.size() > 1)
                    return instance()->throwException<Value>("workspace.add takes zero or one argument");
                const Value &v = args.front();
                if (v.type() != Value::Type_Map)
                    return instance()->throwException<Value>("workspace.add argument needs to be an object");

                if (v.contains("type")) {
                    const Value &t = v["type"];
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
                    const Value &s = v["screen"];
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
    workspace->registerFunction("moveTo", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            if (args.isEmpty())
                return Value::undefined();
            Client *client = WindowManager::instance()->focusedClient();
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
    workspace->registerFunction("select", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
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
            Client *focusedClient = WindowManager::instance()->focusedClient();
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
    layout->registerFunction("adjust", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
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
    kbd->registerFunction("set", [this](const Object::SharedPtr&, const List<Value> &args) -> Value {
            if (args.size() != 2)
                return instance()->throwException<Value>("Invalid number of arguments to kbd.set, 2 required");
            const Value &key = args.at(0);
            const Value &func = args.at(1);
            if (key.type() != Value::Type_String)
                return instance()->throwException<Value>("Invalid first argument to kbd.set, needs to be a string");
            if (!isFunction(func))
                return instance()->throwException<Value>("Invalid second argument to kbd.set, needs to be a JS function");
            Keybinding binding(key.toString(), func);
            if (!binding.isValid())
                return instance()->throwException<Value>(String::format<64>("Couldn't parse keybind for %s",
                                                                            key.toString().constData()));
            WindowManager::instance()->bindings().add(binding);
            return Value::undefined();
        });
    kbd->registerFunction("mode", [](const Object::SharedPtr&, const List<Value> &args) -> Value {
            if (args.size() != 2)
                return instance()->throwException<Value>("Invalid number of arguments to kbd.mode, 2 required");
            const Value &key = args.at(0);
            const Value &subs = args.at(1);
            if (!key.isString())
                return instance()->throwException<Value>("Invalid first argument to kbd.mode, needs to be a string");
            if (!subs.isList())
                return instance()->throwException<Value>("Invalid second argument to kbd.mode, needs to be a list of bindings");
            Set<Keybinding> subbindings;
            for (const Value &sub : subs.toList()) {
                if (!sub.isMap())
                    return instance()->throwException<Value>("Invalid entry in kbd.mode array, all arguments needs to be objects");
                const Map<String, Value> &submap = sub.toMap();
                if (!submap.contains("seq"))
                    return instance()->throwException<Value>("Invalid entry in kbd.mode array, need a seq property");
                if (!submap.contains("action"))
                    return instance()->throwException<Value>("Invalid entry in kbd.mode array, need an action property");
                const Value &seq = submap["seq"];
                const Value &act = submap["action"];
                if (!seq.isString())
                    return instance()->throwException<Value>("Invalid entry in kbd.mode array, seq needs to be a string");
                if (!act.isCustom())
                    return instance()->throwException<Value>("Invalid entry in kbd.mode array, act needs to be a JS function");
                subbindings.insert(Keybinding(seq.toString(), act));
            }
            if (subbindings.isEmpty())
                return instance()->throwException<Value>("Need at least one keybinding in the array for kbd.mode");
            Keybinding toggle(key.toString());
            WindowManager::instance()->bindings().toggle(toggle, subbindings);
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
    if (auto eventLoop = EventLoop::eventLoop()) {
        for (int id : mActiveTimers) {
            eventLoop->unregisterTimer(id);
        }
    }
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

void JavaScript::onClient(Client *client)
{
    mClients.append(client);
    onClientEvent(client, "client");
}

void JavaScript::onClientEvent(Client *client, const String &event)
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

Value JavaScript::startTimer(const List<Value> &args, unsigned int flags)
{
    if (args.size() != 2 || !isFunction(args[0]) || args[1].type() != Value::Type_Integer) {
        return throwException<Value>("Invalid arguments to setTimeout/setInterval");
    }
    const int interval = args[1].toInteger();
    if (interval < 0)
        return instance()->throwException<Value>("Invalid arguments to setTimeout/setInterval");
    std::shared_ptr<ScriptEngine::Object> func = toObject(args[0]);
    return EventLoop::eventLoop()->registerTimer([this, func](int id) {
            assert(func->isFunction());
            func->call();
        }, interval, flags);
}

Value JavaScript::clearTimer(const List<Value> &args)
{
    if (args.size() != 1 || args.first().type() != Value::Type_Integer)
        return instance()->throwException<Value>("Invalid arguments to setTimeout");
    EventLoop::eventLoop()->unregisterTimer(args.first().toInteger());
    return Value::undefined();
}
