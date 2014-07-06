#include "WindowManager.h"
#include "Atoms.h"
#include "Client.h"
#include "Handlers.h"
#include "Types.h"
#include <rct/EventLoop.h>
#include <rct/Message.h>
#include <rct/Messages.h>
#include <rct/Connection.h>
#include <rct/SocketClient.h>
#include <rct/Rct.h>
#include <rct/Log.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <getopt.h>
#include <signal.h>
// Really XCB?? This is awful, awful!
#define explicit _explicit
#include <xcb/xkb.h>
#undef explicit

Path crashHandlerSocketPath;

static void crashHandler(int)
{
    Path::rm(crashHandlerSocketPath);
    _exit(666);
}

typedef union {
    /* All XKB events share these fields. */
    struct {
        uint8_t response_type;
        uint8_t xkbType;
        uint16_t sequence;
        xcb_timestamp_t time;
        uint8_t deviceID;
    } any;
    xcb_xkb_map_notify_event_t map_notify;
    xcb_xkb_state_notify_event_t state_notify;
} _xkb_event;

static inline void handleXkb(_xkb_event* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (event->any.deviceID == wm->xkbDevice()) {
        switch (event->any.xkbType) {
        case XCB_XKB_STATE_NOTIFY:
            wm->updateXkbState(&event->state_notify);
            break;
        case XCB_XKB_MAP_NOTIFY:
            wm->updateXkbMap(&event->map_notify);
            break;
        }
    }
}

class JavascriptMessage : public Message
{
public:
    enum { MessageId = 100 };
    JavascriptMessage(const List<String> &scripts = List<String>())
        : Message(MessageId), mScripts(scripts)
    {}

    List<String> scripts() const { return mScripts; }

    virtual void encode(Serializer &serializer) const { serializer << mScripts; }
    virtual void decode(Deserializer &deserializer) { deserializer >> mScripts; }
private:
    List<String> mScripts;
};

WindowManager *WindowManager::sInstance;

WindowManager::WindowManager()
    : mConn(0), mEwmhConn(0), mScreen(0), mScreenNo(0), mXkbEvent(0), mSyms(0), mTimestamp(XCB_CURRENT_TIME),
      mMoveModifierMask(0), mIsMoving(false), mFocusPolicy(FocusFollowsMouse)
{
    Messages::registerMessage<JavascriptMessage>();
    memset(&mXkb, '\0', sizeof(mXkb));
}

static inline void usage(FILE *out)
{
    fprintf(out,
            "nwm [...options...]\n"
            "  -h|--help                   Display this help\n"
            "  -v|--verbose                Be more verbose\n"
            "  -S|--silent                 Don't log\n"
            "  -l|--logfile [file]         Log to this file\n"
            "  -c|--config [file]          Use this config file instead of ~/.config/nwm.js\n"
            "  -N|--no-system-config       Don't load /etc/xdg/nwm.js\n"
            "  -d|--display [display]      Use this display\n"
            "  -s|--socket-path [path]     Unix socket path (default ~/.nwm.sock)\n"
            "  -j|--javascript [code]      Evaluate javascript remotely\n"
            "  -J|--javascript-file [file] Evaluate javascript remotely from file\n"
            "  -t|--connect-timeout [ms]   Max time to wait for connection\n"
            "  -n|--no-user-config         Don't load ~/.config/nwm.js\n");
}

bool WindowManager::init(int &argc, char **argv)
{
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGBUS, crashHandler);
    sInstance = this;
    struct option opts[] = {
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'b' },
        { "config-file", required_argument, 0, 'c' },
        { "no-system-config", no_argument, 0, 'N' },
        { "no-user-config", no_argument, 0, 'n' },
        { "silent", no_argument, 0, 'S' },
        { "logfile", required_argument, 0, 'l' },
        { "display", required_argument, 0, 'd' },
        { "socket-path", required_argument, 0, 's' },
        { "javascript", required_argument, 0, 'j' },
        { "javascript-file", required_argument, 0, 'J' },
        { "connect-timeout", required_argument, 0, 't' },
        { 0, no_argument, 0, 0 }
    };

    const String shortOptions = Rct::shortOptions(opts);

    List<Path> configFiles;
    bool systemConfig = true;
    bool userConfig = true;
    int logLevel = 0;
    char *logFile = 0;
    char *display = 0;
    Path socketPath = Path::home() + ".nwm.sock";
    List<String> scripts;
    int connectTimeout = 0;

    while (true) {
        const int c = getopt_long(argc, argv, shortOptions.constData(), opts, 0);
        if (c == -1)
            break;
        switch (c) {
        case 'h':
            usage(stdout);
            exit(0);
        case 't': {
            bool ok;
            connectTimeout = String(optarg).toLong(&ok);
            if (!ok || connectTimeout < 0) {
                fprintf(stderr, "%s is not a valid positive integer", optarg);
                return false;
            }
            break; }
        case 's':
            socketPath = optarg;
            break;
        case 'J': {
            const Path file(optarg);
            if (!file.isFile()) {
                fprintf(stderr, "%s doesn't seem to be a file\n", optarg);
                return false;
            }
            scripts.append(file.readAll());
            break; }
        case 'j': {
            scripts.append(optarg);
            break; }
        case 'n':
            userConfig = false;
            break;
        case 'S':
            logLevel = -1;
            break;
        case 'd':
            display = optarg;
            break;
        case 'v':
            if (logLevel >= 0)
                ++logLevel;
            break;
        case 'N':
            systemConfig = false;
            break;
        case 'l':
            logFile = optarg;
            break;
        case 'c': {
            userConfig = false;
            const Path cfg(optarg);
            if (!cfg.isFile()) {
                fprintf(stderr, "%s doesn't seem to exist\n", optarg);
                return false;
            }
            configFiles << cfg;
            break; }
        default:
            break;
        }
    }

    if (!initLogging(argv[0], LogStderr, logLevel, logFile, 0)) {
        fprintf(stderr, "Can't initialize logging\n");
        return false;
    }

    if (userConfig)
        configFiles << Path::home() + ".config/nwm.js";
    if (systemConfig)
        configFiles << "/etc/xdg/nwm.js";

    if (display) {
        mDisplay = display;
    } else if (mDisplay.isEmpty()) {
        mDisplay = getenv("DISPLAY");
    }
    if (mDisplay.isEmpty()) {
        error() << "No display set";
        return false;
    }

    assert(!mDisplay.isEmpty());
    mConn = xcb_connect(mDisplay.constData(), &mScreenNo);
    if (!mConn || xcb_connection_has_error(mConn)) {
        return false;
    }
    mEwmhConn = new xcb_ewmh_connection_t;
    xcb_intern_atom_cookie_t* ewmhCookies = xcb_ewmh_init_atoms(mConn, mEwmhConn);
    if (!ewmhCookies) {
        error() << "unable to init ewmh";
        xcb_disconnect(mConn);
        mConn = 0;
        delete mEwmhConn;
        mEwmhConn = 0;
        return false;
    }
    if (!xcb_ewmh_init_atoms_replies(mEwmhConn, ewmhCookies, 0)) {
        error() << "unable to get ewmh reply";
        xcb_disconnect(mConn);
        mConn = 0;
        delete mEwmhConn;
        mEwmhConn = 0;
        return false;
    }

    mScreen = xcb_aux_get_screen(mConn, mScreenNo);

    if (!isRunning()) {
        ServerGrabScope scope(mConn);

        if (!install()) {
            error() << "Unable to install nwm. Another window manager already running?";
            return false;
        }
        ::crashHandlerSocketPath = socketPath;

        mJS.init();
        for (int i=configFiles.size() - 1; i>=0; --i) {
            const String contents = configFiles[i].readAll();
            if (!contents.isEmpty()) {
                String err;
                mJS.evaluate(contents, configFiles[i], &err);
                if (!err.isEmpty()) {
                    error() << err;
                    return false;
                }
            }
        }

        if (mWorkspaces.isEmpty()) {
            error() << "No workspaces";
            return false;
        }

        mWorkspaces[0]->activate();
        // update ewmh
        xcb_ewmh_set_number_of_desktops(mEwmhConn, mScreenNo, mWorkspaces.size());
        xcb_ewmh_set_current_desktop(mEwmhConn, mScreenNo, 0);

        if (!manage()) {
            error() << "Unable to manage existing windows";
            return false;
        }
        for (const auto &script : scripts) {
            String err;
            mJS.evaluate(script, "<message>", &err);
            if (!err.isEmpty()) {
                error("Javascript error: %s", err.constData());
                return false;
            }
        }
        mServer.newConnection().connect([this](SocketServer *server) {
                SocketClient::SharedPtr client;
                while ((client = server->nextConnection())) {
                    Connection *conn = new Connection(client);
                    conn->newMessage().connect([this](Message *msg, Connection *c) {
                            if (msg->messageId() != JavascriptMessage::MessageId) {
                                c->write<128>("Invalid message id: %d", msg->messageId());
                                c->finish();
                                return;
                            }
                            const List<String> scripts = static_cast<JavascriptMessage*>(msg)->scripts();
                            for (const auto &script : scripts) {
                                String error;
                                const Value ret = mJS.evaluate(script, "<message>", &error);
                                if (!error.isEmpty()) {
                                    c->write<128>("Javascript error: %s", error.constData());
                                } else {
                                    c->write<128>("%s", ret.toJSON(true).constData());
                                }
                            }
                            c->finish();
                        });

                    conn->disconnected().connect([](Connection *c) {
                            c->disconnected().disconnect();
                            EventLoop::deleteLater(c);
                        });
                }
            });
        mServer.listen(socketPath);
        return true;
    } else if (scripts.isEmpty()) {
        error() << "nwm is already running on display" << mDisplay;
        return false;
    } else {
        Connection *connection = new Connection;
        connection->newMessage().connect([](Message *msg, Connection *c) {
                if (msg->messageId() != Message::ResponseId) {
                    error() << "Invalid message" << msg->messageId();
                    c->close();
                    EventLoop::eventLoop()->quit();
                    return;
                }
                printf("%s\n", static_cast<ResponseMessage*>(msg)->data().constData());
            });
        connection->finished().connect([](Connection *, int){ EventLoop::eventLoop()->quit(); });
        connection->disconnected().connect([](Connection *){ EventLoop::eventLoop()->quit(); });
        if (!connection->connectUnix(socketPath, connectTimeout)) {
            delete connection;
            error("Can't seem to connect to server");
            return false;
        }
        connection->send(JavascriptMessage(scripts));
        return true;
    }

    return true;
}

WindowManager::~WindowManager()
{
    assert(sInstance == this);
    sInstance = 0;
    if (mXkb.ctx) {
        xkb_state_unref(mXkb.state);
        xkb_keymap_unref(mXkb.keymap);
        xkb_context_unref(mXkb.ctx);
    }
    if (mSyms)
        xcb_key_symbols_free(mSyms);

    Client::clear();
    if (mConn) {
        if (EventLoop::SharedPtr eventLoop = EventLoop::eventLoop()) {
            const int fd = xcb_get_file_descriptor(mConn);
            eventLoop->unregisterSocket(fd);
        }

        if (mEwmhConn) {
            xcb_ewmh_connection_wipe(mEwmhConn);
            delete mEwmhConn;
            mEwmhConn = 0;
        }

        xcb_disconnect(mConn);
        mConn = 0;
    } else {
        assert(!mEwmhConn);
    }
}

bool WindowManager::manage()
{
    // Manage all existing windows
    xcb_generic_error_t* err;
    xcb_query_tree_cookie_t treeCookie = xcb_query_tree(mConn, mScreen->root);
    xcb_query_tree_reply_t* treeReply = xcb_query_tree_reply(mConn, treeCookie, &err);
    FreeScope scope(treeReply);
    if (err) {
        LOG_ERROR(err, "Unable to query window tree");
        free(err);
        return false;
    }
    xcb_window_t* clients = xcb_query_tree_children(treeReply);
    if (clients) {
        const int clientLength = xcb_query_tree_children_length(treeReply);

        std::vector<xcb_get_window_attributes_cookie_t> attrs;
        std::vector<xcb_get_property_cookie_t> states;
        attrs.reserve(clientLength);
        states.reserve(clientLength);

        for (int i = 0; i < clientLength; ++i) {
            attrs.push_back(xcb_get_window_attributes_unchecked(mConn, clients[i]));
            states.push_back(xcb_get_property_unchecked(mConn, false, clients[i], Atoms::WM_STATE, Atoms::WM_STATE, 0L, 2L));
        }
        for (int i = 0; i < clientLength; ++i) {
            xcb_get_window_attributes_reply_t* attr = xcb_get_window_attributes_reply(mConn, attrs[i], 0);
            FreeScope scope(attr);
            xcb_get_property_reply_t* state = xcb_get_property_reply(mConn, states[i], 0);
            uint32_t stateValue = XCB_ICCCM_WM_STATE_NORMAL;
            if (state) {
                if (xcb_get_property_value_length(state))
                    stateValue = *static_cast<uint32_t*>(xcb_get_property_value(state));
                free(state);
            }

            if (!attr || attr->override_redirect || attr->map_state == XCB_MAP_STATE_UNMAPPED
                || stateValue == XCB_ICCCM_WM_STATE_WITHDRAWN) {
                continue;
            }
            Client::SharedPtr client = Client::manage(clients[i]);
        }
    }
    return true;
}

bool WindowManager::install()
{
    mRect = Rect({ 0, 0, mScreen->width_in_pixels, mScreen->height_in_pixels });

    Atoms::setup(mConn);

    xcb_void_cookie_t cookie;
    xcb_generic_error_t* err;

    // check if another WM is running
    {
        const uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
        cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(mConn, cookie);
        if (err) {
            LOG_ERROR(err, "Unable to change window attributes 1");
            free(err);
            return false;
        }
    }

    // lookup keyboard stuffs
    {
        xcb_prefetch_extension_data(mConn, &xcb_xkb_id);

        const int ret = xkb_x11_setup_xkb_extension(mConn,
                                                    XKB_X11_MIN_MAJOR_XKB_VERSION,
                                                    XKB_X11_MIN_MINOR_XKB_VERSION,
                                                    XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
                                                    0, 0, 0, 0);
        if (!ret) {
            error() << "Unable to setup XKB extension";
            return false;
        }
        xkb_context* ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!ctx) {
            error() << "Unable to create new xkb_context";
            return false;
        }
        const int32_t deviceId = xkb_x11_get_core_keyboard_device_id(mConn);
        if (deviceId == -1) {
            error() << "Unable to get device id from core keyboard device";
            xkb_context_unref(ctx);
            return false;
        }
        xkb_keymap* keymap = xkb_x11_keymap_new_from_device(ctx, mConn, deviceId, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (!keymap) {
            error() << "Unable to get keymap from device";
            xkb_context_unref(ctx);
            return false;
        }
        xkb_state* state = xkb_x11_state_new_from_device(keymap, mConn, deviceId);
        if (!state) {
            error() << "Unable to get state from keymap/device";
            xkb_keymap_unref(keymap);
            xkb_context_unref(ctx);
            return false;
        }

        const xcb_query_extension_reply_t *reply = xcb_get_extension_data(mConn, &xcb_xkb_id);
        if (!reply || !reply->present) {
            error() << "Unable to get extension reply for XKB";
            xkb_state_unref(state);
            xkb_keymap_unref(keymap);
            xkb_context_unref(ctx);
            return false;
        }

        unsigned int affectMap, map;
        affectMap = map = XCB_XKB_MAP_PART_KEY_TYPES
            | XCB_XKB_MAP_PART_KEY_SYMS
            | XCB_XKB_MAP_PART_MODIFIER_MAP
            | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
            | XCB_XKB_MAP_PART_KEY_ACTIONS
            | XCB_XKB_MAP_PART_KEY_BEHAVIORS
            | XCB_XKB_MAP_PART_VIRTUAL_MODS
            | XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;

        xcb_void_cookie_t select = xcb_xkb_select_events_checked(mConn, XCB_XKB_ID_USE_CORE_KBD,
                                                                 XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY,
                                                                 0,
                                                                 XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY,
                                                                 affectMap, map, 0);
        err = xcb_request_check(mConn, select);
        if (err) {
            LOG_ERROR(err, "Unable to select XKB events");
            free(err);
            xkb_state_unref(state);
            xkb_keymap_unref(keymap);
            xkb_context_unref(ctx);
            return false;
        }

        mXkbEvent = reply->first_event;
        mXkb = Xkb({ ctx, keymap, state, deviceId });

        mSyms = xcb_key_symbols_alloc(mConn);
    }

    const uint32_t values[] = { Types::RootEventMask };
    cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
    err = xcb_request_check(mConn, cookie);
    if (err) {
        LOG_ERROR(err, "Unable to change window attributes 2");
        free(err);
        return false;
    }

    const xcb_atom_t atom[] = {
        mEwmhConn->_NET_SUPPORTED,
        // Atoms::_NET_SUPPORTING_WM_CHECK,
        // Atoms::_NET_STARTUP_ID,
        // Atoms::_NET_CLIENT_LIST,
        // Atoms::_NET_CLIENT_LIST_STACKING,
        mEwmhConn->_NET_NUMBER_OF_DESKTOPS,
        mEwmhConn->_NET_CURRENT_DESKTOP,
        // Atoms::_NET_DESKTOP_NAMES,
        // Atoms::_NET_ACTIVE_WINDOW,
        // Atoms::_NET_CLOSE_WINDOW,
        // Atoms::_NET_WM_NAME,
        mEwmhConn->_NET_WM_STRUT,
        mEwmhConn->_NET_WM_STRUT_PARTIAL,
        // Atoms::_NET_WM_ICON_NAME,
        // Atoms::_NET_WM_VISIBLE_ICON_NAME,
        // Atoms::_NET_WM_DESKTOP,
        // Atoms::_NET_WM_WINDOW_TYPE,
        // Atoms::_NET_WM_WINDOW_TYPE_DESKTOP,
        // Atoms::_NET_WM_WINDOW_TYPE_DOCK,
        // Atoms::_NET_WM_WINDOW_TYPE_TOOLBAR,
        // Atoms::_NET_WM_WINDOW_TYPE_MENU,
        // Atoms::_NET_WM_WINDOW_TYPE_UTILITY,
        // Atoms::_NET_WM_WINDOW_TYPE_SPLASH,
        // Atoms::_NET_WM_WINDOW_TYPE_DIALOG,
        // Atoms::_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        // Atoms::_NET_WM_WINDOW_TYPE_POPUP_MENU,
        // Atoms::_NET_WM_WINDOW_TYPE_TOOLTIP,
        // Atoms::_NET_WM_WINDOW_TYPE_NOTIFICATION,
        // Atoms::_NET_WM_WINDOW_TYPE_COMBO,
        // Atoms::_NET_WM_WINDOW_TYPE_DND,
        // Atoms::_NET_WM_WINDOW_TYPE_NORMAL,
        // Atoms::_NET_WM_ICON,
        // Atoms::_NET_WM_PID,
        mEwmhConn->_NET_WM_STATE,
        mEwmhConn->_NET_WM_STATE_STICKY
        // Atoms::_NET_WM_STATE_SKIP_TASKBAR,
        // Atoms::_NET_WM_STATE_FULLSCREEN,
        // Atoms::_NET_WM_STATE_MAXIMIZED_HORZ,
        // Atoms::_NET_WM_STATE_MAXIMIZED_VERT,
        // Atoms::_NET_WM_STATE_ABOVE,
        // Atoms::_NET_WM_STATE_BELOW,
        // Atoms::_NET_WM_STATE_MODAL,
        // Atoms::_NET_WM_STATE_HIDDEN,
        // Atoms::_NET_WM_STATE_DEMANDS_ATTENTION
    };

    cookie = xcb_change_property(mConn, XCB_PROP_MODE_REPLACE,
                                 mScreen->root, mEwmhConn->_NET_SUPPORTED, XCB_ATOM_ATOM, 32,
                                 Rct::countof(atom), atom);
    err = xcb_request_check(mConn, cookie);
    if (err) {
        LOG_ERROR(err, "Unable to change _NET_SUPPORTED property on root window");
        free(err);
        return false;
    }

    xcb_ewmh_set_wm_pid(mEwmhConn, mScreen->root, getpid());
    xcb_flush(mConn);

    // Get events
    xcb_connection_t* conn = mConn;
    const int fd = xcb_get_file_descriptor(mConn);
    uint8_t xkbEvent = mXkbEvent;
    EventLoop::eventLoop()->registerSocket(fd, EventLoop::SocketRead, [conn, fd, xkbEvent](int, unsigned int) {
            for (;;) {
                if (xcb_connection_has_error(conn)) {
                    error() << "X server connection error" << xcb_connection_has_error(conn);
                    if (EventLoop::SharedPtr eventLoop = EventLoop::eventLoop()) {
                        eventLoop->unregisterSocket(fd);
                        eventLoop->quit();
                    }
                    return;
                }
                xcb_generic_event_t* event = xcb_poll_for_event(conn);
                if (event) {
                    FreeScope scope(event);
                    const unsigned int responseType = event->response_type & ~0x80;
                    switch (responseType) {
                    case XCB_BUTTON_PRESS:
                        error() << "button press";
                        Handlers::handleButtonPress(reinterpret_cast<xcb_button_press_event_t*>(event));
                        break;
                    case XCB_BUTTON_RELEASE:
                        error() << "button release";
                        Handlers::handleButtonRelease(reinterpret_cast<xcb_button_release_event_t*>(event));
                        break;
                    case XCB_MOTION_NOTIFY:
                        error() << "motion notify";
                        Handlers::handleMotionNotify(reinterpret_cast<xcb_motion_notify_event_t*>(event));
                        break;
                    case XCB_CLIENT_MESSAGE:
                        error() << "client message";
                        Handlers::handleClientMessage(reinterpret_cast<xcb_client_message_event_t*>(event));
                        break;
                    case XCB_CONFIGURE_REQUEST:
                        error() << "configure request";
                        Handlers::handleConfigureRequest(reinterpret_cast<xcb_configure_request_event_t*>(event));
                        break;
                    case XCB_CONFIGURE_NOTIFY:
                        error() << "configure notify";
                        Handlers::handleConfigureNotify(reinterpret_cast<xcb_configure_notify_event_t*>(event));
                        break;
                    case XCB_DESTROY_NOTIFY:
                        error() << "destroy notify";
                        Handlers::handleDestroyNotify(reinterpret_cast<xcb_destroy_notify_event_t*>(event));
                        break;
                    case XCB_ENTER_NOTIFY:
                        error() << "enter notify";
                        Handlers::handleEnterNotify(reinterpret_cast<xcb_enter_notify_event_t*>(event));
                        break;
                    case XCB_EXPOSE:
                        error() << "expose";
                        Handlers::handleExpose(reinterpret_cast<xcb_expose_event_t*>(event));
                        break;
                    case XCB_FOCUS_IN:
                        error() << "focus in";
                        Handlers::handleFocusIn(reinterpret_cast<xcb_focus_in_event_t*>(event));
                        break;
                    case XCB_KEY_PRESS:
                        error() << "key press";
                        Handlers::handleKeyPress(reinterpret_cast<xcb_key_press_event_t*>(event));
                        break;
                    case XCB_MAP_REQUEST:
                        error() << "map request";
                        Handlers::handleMapRequest(reinterpret_cast<xcb_map_request_event_t*>(event));
                        break;
                    case XCB_PROPERTY_NOTIFY:
                        error() << "property notify";
                        Handlers::handlePropertyNotify(reinterpret_cast<xcb_property_notify_event_t*>(event));
                        break;
                    case XCB_UNMAP_NOTIFY:
                        error() << "unmap notify";
                        Handlers::handleUnmapNotify(reinterpret_cast<xcb_unmap_notify_event_t*>(event));
                        break;
                    default:
                        if (responseType == xkbEvent) {
                            error() << "xkb event";
                            handleXkb(reinterpret_cast<_xkb_event*>(event));
                            break;
                        }
                        error() << "unhandled event" << responseType;
                        break;
                    }
                } else {
                    break;
                }
            }
            xcb_flush(conn);
        });

    return true;
}

bool WindowManager::isRunning()
{
    xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_pid(mEwmhConn, mScreen->root);
    uint32_t pid;
    if (!xcb_ewmh_get_wm_pid_reply(mEwmhConn, cookie, &pid, 0))
        return false;

    return !kill(pid, 0);
}

String WindowManager::keycodeToString(xcb_keycode_t code)
{
    const int size = xkb_state_key_get_utf8(mXkb.state, code, 0, 0);
    if (size <= 0)
        return String();
    String str(size, '\0');
    xkb_state_key_get_utf8(mXkb.state, code, str.data(), size + 1);
    return str;
}

xkb_keysym_t WindowManager::keycodeToKeysym(xcb_keycode_t code)
{
    return xkb_state_key_get_one_sym(mXkb.state, code);
}

void WindowManager::updateXkbState(xcb_xkb_state_notify_event_t* state)
{
    xkb_state_update_mask(mXkb.state,
                          state->baseMods,
                          state->latchedMods,
                          state->lockedMods,
                          state->baseGroup,
                          state->latchedGroup,
                          state->lockedGroup);
}

void WindowManager::updateXkbMap(xcb_xkb_map_notify_event_t* map)
{
    assert(mSyms);
    xcb_key_symbols_free(mSyms);
    mSyms = xcb_key_symbols_alloc(mConn);
    mBindings.rebindAll();
#warning requery xkb core device and recreate state here?
}

void WindowManager::setRect(const Rect& rect)
{
    mRect = rect;
    for (const Workspace::SharedPtr& ws : mWorkspaces) {
        ws->setRect(rect);
    }
}

void WindowManager::addWorkspace(unsigned int layoutType)
{
    mWorkspaces.append(std::make_shared<Workspace>(layoutType, mRect));
}

void WindowManager::setMoveModifier(const String& mod)
{
    mMoveModifier = mod;
    mMoveModifierMask = Keybinding::modToMask(mod);
}
