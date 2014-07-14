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
    if (!crashHandlerSocketPath.isEmpty())
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
    WindowManager *wm = WindowManager::instance();
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

class NWMMessage : public Message
{
public:
    enum { MessageId = 100 };
    NWMMessage()
        : Message(MessageId), mFlags(0)
    {}

    unsigned int flags() const { return mFlags; }
    enum Flag {
        Restart = 0x01,
        Reload = 0x02,
        Quit = 0x04
    };

    List<String> scripts() const { return mScripts; }
    void setScripts(const List<String> &scripts) { mScripts = scripts; }
    void setFlag(Flag flag, bool on = true) { if (on) { mFlags |= flag; } else { mFlags &= ~flag; } }
    void setFlags(unsigned int flags) { mFlags = flags; }

    virtual void encode(Serializer &serializer) const { serializer << mScripts << mFlags; }
    virtual void decode(Deserializer &deserializer) { deserializer >> mScripts >> mFlags; }
private:
    List<String> mScripts;
    unsigned int mFlags;
};

WindowManager *WindowManager::sInstance;

WindowManager::WindowManager()
    : mConn(0), mEwmhConn(0), mPreferredScreenIndex(0), mXkbEvent(0), mSyms(0), mTimestamp(XCB_CURRENT_TIME),
      mMoveModifierMask(0), mIsMoving(false), mFocusPolicy(FocusFollowsMouse), mCurrentScreen(-1), mRestart(false)
{
    Messages::registerMessage<NWMMessage>();
    memset(&mXkb, '\0', sizeof(mXkb));
    assert(!sInstance);
    sInstance = this;
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
            "  -r|--reload                 Reload config files\n"
            "  -R|--restart                Restart window manager\n"
            "  -q|--quit                   Stop window manager\n"
            "  -n|--no-user-config         Don't load ~/.config/nwm.js\n");
}

bool WindowManager::init(int &argc, char **argv)
{
    static bool first = true;
    if (first) {
        signal(SIGSEGV, crashHandler);
        signal(SIGABRT, crashHandler);
        signal(SIGBUS, crashHandler);
        first = false;
    } else {
        optind = 1; // for reinit
    }

    struct option opts[] = {
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'v' },
        { "config-file", required_argument, 0, 'c' },
        { "no-system-config", no_argument, 0, 'N' },
        { "no-user-config", no_argument, 0, 'n' },
        { "silent", no_argument, 0, 'S' },
        { "logfile", required_argument, 0, 'l' },
        { "display", required_argument, 0, 'd' },
        { "socket-path", required_argument, 0, 's' },
        { "javascript", required_argument, 0, 'j' },
        { "javascript-file", required_argument, 0, 'J' },
        { "quit", no_argument, 0, 'q' },
        { "reload", no_argument, 0, 'r' },
        { "restart", no_argument, 0, 'R' },
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
    Path socketPath;
    List<String> scripts;
    unsigned int flags = 0;
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
        case 'r':
            flags |= NWMMessage::Reload;
            break;
        case 'q':
            flags |= NWMMessage::Quit;
            break;
        case 'R':
            flags |= NWMMessage::Restart;
            break;
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

    String displayStr;
    if (display) {
        displayStr = display;
    } else if (displayStr.isEmpty()) {
        displayStr = getenv("DISPLAY");
    }
    if (displayStr.isEmpty()) {
        error() << "No display set";
        return false;
    }

    assert(!displayStr.isEmpty());
    mConn = xcb_connect(displayStr.constData(), &mPreferredScreenIndex);

    if (!mConn || xcb_connection_has_error(mConn)) {
        return false;
    }
    const xcb_setup_t *setup = xcb_get_setup(mConn);
    if (setup) {
        warning() << "status" << static_cast<uint32_t>(setup->status) << '\n'
                  << "pad0" << static_cast<uint32_t>(setup->pad0) << '\n'
                  << "protocol_major_version" << static_cast<uint32_t>(setup->protocol_major_version) << '\n'
                  << "protocol_minor_version" << static_cast<uint32_t>(setup->protocol_minor_version) << '\n'
                  << "length" << static_cast<uint32_t>(setup->length) << '\n'
                  << "release_number" << static_cast<uint32_t>(setup->release_number) << '\n'
                  << "resource_id_base" << static_cast<uint32_t>(setup->resource_id_base) << '\n'
                  << "resource_id_mask" << static_cast<uint32_t>(setup->resource_id_mask) << '\n'
                  << "motion_buffer_size" << static_cast<uint32_t>(setup->motion_buffer_size) << '\n'
                  << "vendor_len" << static_cast<uint32_t>(setup->vendor_len) << '\n'
                  << "maximum_request_length" << static_cast<uint32_t>(setup->maximum_request_length) << '\n'
                  << "roots_len" << static_cast<uint32_t>(setup->roots_len) << '\n'
                  << "pixmap_formats_len" << static_cast<uint32_t>(setup->pixmap_formats_len) << '\n'
                  << "image_byte_order" << static_cast<uint32_t>(setup->image_byte_order) << '\n'
                  << "bitmap_format_bit_order" << static_cast<uint32_t>(setup->bitmap_format_bit_order) << '\n'
                  << "bitmap_format_scanline_unit" << static_cast<uint32_t>(setup->bitmap_format_scanline_unit) << '\n'
                  << "bitmap_format_scanline_pad" << static_cast<uint32_t>(setup->bitmap_format_scanline_pad) << '\n'
                  << "min_keycode" << static_cast<uint32_t>(setup->min_keycode) << '\n'
                  << "max_keycode" << static_cast<uint32_t>(setup->max_keycode);
        // << "pad1[4]" << static_cast<uint32>(setup->pad1[4)];
    }

    const int screenCount = xcb_setup_roots_length(setup);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    mScreens.resize(screenCount);
    for (int i=0; i<screenCount; ++i) {
        mScreens[i].screen = it.data;
        mScreens[i].visual = xcb_aux_get_visualtype(mConn, i, it.data->root_visual);
        xcb_screen_next(&it);
    }
    error() << "Got screens" << screenCount;

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


    if (socketPath.isEmpty())
        socketPath = Path::home() + ".nwm.sock." + displayStr;

    mDisplay = displayStr;
    // strip out the screen part if we need to
    if (mScreens.size() > 1) {
        const int dotIdx = mDisplay.lastIndexOf('.');
        const int colonIdx = mDisplay.lastIndexOf(':');
        assert(colonIdx != -1);
        if (dotIdx != -1 && dotIdx > colonIdx) {
            mDisplay.truncate(dotIdx);
        }
    }


    if (!isRunning()) {
        ServerGrabScope scope(mConn);

        if (!install()) {
            error() << "Unable to install nwm. Another window manager already running?";
            return false;
        }

        String err;
        if (!mJS.init(configFiles, &err)) {
            error() << err;
            return false;
        }

        // update ewmh
        for (int i=0; i<screenCount; ++i) {
            if (mScreens[i].workspaces.isEmpty()) {
                error() << "No workspaces for screen" << i;
                return false;
            }
            mScreens[i].workspaces.first()->activate();
            xcb_ewmh_set_number_of_desktops(mEwmhConn, i, mScreens.at(i).workspaces.size());
            xcb_ewmh_set_current_desktop(mEwmhConn, i, 0);
        }

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
                            if (msg->messageId() != NWMMessage::MessageId) {
                                c->write<128>("Invalid message id: %d", msg->messageId());
                                c->finish();
                                return;
                            }
                            const NWMMessage *m = static_cast<NWMMessage*>(msg);
                            if (m->flags() & NWMMessage::Restart) {
                                restart();
                            }

                            if (m->flags() & NWMMessage::Quit) {
                                EventLoop::eventLoop()->quit();
                            }

                            String err;
                            if (m->flags() & NWMMessage::Reload && !mJS.reload(&err)) {
                                c->write<128>("Error in init file(s): %s", err.constData());
                                EventLoop::eventLoop()->quit();
                            }

                            for (const auto &script : m->scripts()) {
                                String error;
                                const Value ret = mJS.evaluate(script, "<message>", &error);
                                if (!error.isEmpty()) {
                                    c->write<128>("Javascript error: %s", error.constData());
                                } else {
                                    c->write(ret.toString());
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
        Path::rm(socketPath);
        ::crashHandlerSocketPath = socketPath;
        mServer.listen(socketPath);
        return true;
    } else if (scripts.isEmpty() && !flags) {
        error() << "nwm is already running on display" << displayStr;
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
        NWMMessage msg;
        msg.setScripts(scripts);
        msg.setFlags(flags);
        connection->send(msg);
        return true;
    }
    return true;
}

WindowManager::~WindowManager()
{
    Client::clear();
    mJS.clear();
    for (Screen &screen : mScreens) {
        screen.workspaces.deleteAll();
    }
    mScreens.clear();
    if (mXkb.ctx) {
        xkb_state_unref(mXkb.state);
        xkb_keymap_unref(mXkb.keymap);
        xkb_context_unref(mXkb.ctx);
    }
    if (mSyms)
        xcb_key_symbols_free(mSyms);

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
    assert(sInstance == this);
    sInstance = 0;
}

bool WindowManager::manage()
{
    // Manage all existing windows
    int screenNumber = 0;
    for (const auto &it : mScreens) {
        AutoPointer<xcb_generic_error_t> err;
        xcb_query_tree_cookie_t treeCookie = xcb_query_tree(mConn, it.screen->root);
        AutoPointer<xcb_query_tree_reply_t> treeReply(xcb_query_tree_reply(mConn, treeCookie, &err));
        if (err) {
            LOG_ERROR(err, "Unable to query window tree");
            return false;
        }
        xcb_window_t* clients = xcb_query_tree_children(treeReply);
        if (clients) {
            const int clientLength = xcb_query_tree_children_length(treeReply);

            List<xcb_get_window_attributes_cookie_t> attrs;
            List<xcb_get_property_cookie_t> states;
            attrs.reserve(clientLength);
            states.reserve(clientLength);

            warning() << "Got clients" << clientLength << it.screen << screenNumber;

            for (int i = 0; i < clientLength; ++i) {
                attrs.push_back(xcb_get_window_attributes_unchecked(mConn, clients[i]));
                states.push_back(xcb_get_property_unchecked(mConn, false, clients[i], Atoms::WM_STATE, Atoms::WM_STATE, 0L, 2L));
            }
            for (int i = 0; i < clientLength; ++i) {
                AutoPointer<xcb_generic_error_t> err;
                AutoPointer<xcb_get_window_attributes_reply_t> attr(xcb_get_window_attributes_reply(mConn, attrs[i], &err));
                xcb_get_property_reply_t* state = xcb_get_property_reply(mConn, states[i], 0);
                if (err) {
                    LOG_ERROR(err, "Unable to get attrs");
                }
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
                Client::SharedPtr client = Client::manage(clients[i], screenNumber);
            }
        }
        ++screenNumber;
    }
    return true;
}

bool WindowManager::install()
{
    Atoms::setup(mConn);

    xcb_void_cookie_t cookie;
    AutoPointer<xcb_generic_error_t> err;

    // check if another WM is running
    {
        const uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
        cookie = xcb_change_window_attributes_checked(mConn, mScreens.at(mPreferredScreenIndex).screen->root, XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(mConn, cookie);
        if (err) {
            LOG_ERROR(err, "Unable to change window attributes 1");
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

    const xcb_atom_t atom[] = {
        mEwmhConn->_NET_SUPPORTED,
        // Atoms::_NET_SUPPORTING_WM_CHECK,
        // Atoms::_NET_STARTUP_ID,
        // Atoms::_NET_CLIENT_LIST,
        // Atoms::_NET_CLIENT_LIST_STACKING,
        mEwmhConn->_NET_NUMBER_OF_DESKTOPS,
        mEwmhConn->_NET_CURRENT_DESKTOP,
        // Atoms::_NET_DESKTOP_NAMES,
        mEwmhConn->_NET_ACTIVE_WINDOW,
        // Atoms::_NET_CLOSE_WINDOW,
        // Atoms::_NET_WM_NAME,
        mEwmhConn->_NET_WM_STRUT,
        mEwmhConn->_NET_WM_STRUT_PARTIAL,
        // Atoms::_NET_WM_ICON_NAME,
        // Atoms::_NET_WM_VISIBLE_ICON_NAME,
        // Atoms::_NET_WM_DESKTOP,
        mEwmhConn->_NET_WM_WINDOW_TYPE,
        // Atoms::_NET_WM_WINDOW_TYPE_DESKTOP,
        // Atoms::_NET_WM_WINDOW_TYPE_DOCK,
        // Atoms::_NET_WM_WINDOW_TYPE_TOOLBAR,
        // Atoms::_NET_WM_WINDOW_TYPE_MENU,
        mEwmhConn->_NET_WM_WINDOW_TYPE_UTILITY,
        // Atoms::_NET_WM_WINDOW_TYPE_SPLASH,
        mEwmhConn->_NET_WM_WINDOW_TYPE_DIALOG,
        // Atoms::_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        // Atoms::_NET_WM_WINDOW_TYPE_POPUP_MENU,
        // Atoms::_NET_WM_WINDOW_TYPE_TOOLTIP,
        // Atoms::_NET_WM_WINDOW_TYPE_NOTIFICATION,
        // Atoms::_NET_WM_WINDOW_TYPE_COMBO,
        // Atoms::_NET_WM_WINDOW_TYPE_DND,
        // Atoms::_NET_WM_WINDOW_TYPE_NORMAL,
        // Atoms::_NET_WM_ICON,
        mEwmhConn->_NET_WM_PID,
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


    for (Screen &s : mScreens) {
        s.rect = { 0, 0, s.screen->width_in_pixels, s.screen->height_in_pixels };
        cookie = xcb_change_window_attributes_checked(mConn, s.screen->root, XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(mConn, cookie);
        if (err) {
            LOG_ERROR(err, "Unable to change window attributes 2");
            return false;
        }

        cookie = xcb_change_property(mConn, XCB_PROP_MODE_REPLACE,
                                     s.screen->root, mEwmhConn->_NET_SUPPORTED, XCB_ATOM_ATOM, 32,
                                     Rct::countof(atom), atom);
        err = xcb_request_check(mConn, cookie);
        if (err) {
            LOG_ERROR(err, "Unable to change _NET_SUPPORTED property on root window");
            return false;
        }

        xcb_ewmh_set_wm_pid(mEwmhConn, s.screen->root, getpid());
    }
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
                AutoPointer<xcb_generic_event_t> event(xcb_poll_for_event(conn));
                if (event) {
                    const unsigned int responseType = event->response_type & ~0x80;
                    switch (responseType) {
                    case XCB_BUTTON_PRESS:
                        warning() << "button press";
                        Handlers::handleButtonPress(event.cast<xcb_button_press_event_t>());
                        break;
                    case XCB_BUTTON_RELEASE:
                        warning() << "button release";
                        Handlers::handleButtonRelease(event.cast<xcb_button_release_event_t>());
                        break;
                    case XCB_MOTION_NOTIFY:
                        warning() << "motion notify";
                        Handlers::handleMotionNotify(event.cast<xcb_motion_notify_event_t>());
                        break;
                    case XCB_CLIENT_MESSAGE:
                        warning() << "client message";
                        Handlers::handleClientMessage(event.cast<xcb_client_message_event_t>());
                        break;
                    case XCB_CONFIGURE_REQUEST:
                        warning() << "configure request";
                        Handlers::handleConfigureRequest(event.cast<xcb_configure_request_event_t>());
                        break;
                    case XCB_CONFIGURE_NOTIFY:
                        warning() << "configure notify";
                        Handlers::handleConfigureNotify(event.cast<xcb_configure_notify_event_t>());
                        break;
                    case XCB_DESTROY_NOTIFY:
                        warning() << "destroy notify";
                        Handlers::handleDestroyNotify(event.cast<xcb_destroy_notify_event_t>());
                        break;
                    case XCB_ENTER_NOTIFY:
                        warning() << "enter notify";
                        Handlers::handleEnterNotify(event.cast<xcb_enter_notify_event_t>());
                        break;
                    case XCB_EXPOSE:
                        warning() << "expose";
                        Handlers::handleExpose(event.cast<xcb_expose_event_t>());
                        break;
                    case XCB_FOCUS_IN:
                        warning() << "focus in";
                        Handlers::handleFocusIn(event.cast<xcb_focus_in_event_t>());
                        break;
                    case XCB_KEY_PRESS:
                        warning() << "key press";
                        Handlers::handleKeyPress(event.cast<xcb_key_press_event_t>());
                        break;
                    case XCB_MAP_REQUEST:
                        warning() << "map request";
                        Handlers::handleMapRequest(event.cast<xcb_map_request_event_t>());
                        break;
                    case XCB_PROPERTY_NOTIFY:
                        warning() << "property notify";
                        Handlers::handlePropertyNotify(event.cast<xcb_property_notify_event_t>());
                        break;
                    case XCB_UNMAP_NOTIFY:
                        warning() << "unmap notify";
                        Handlers::handleUnmapNotify(event.cast<xcb_unmap_notify_event_t>());
                        break;
                    default:
                        if (responseType == xkbEvent) {
                            warning() << "xkb event";
                            handleXkb(event.cast<_xkb_event>());
                            break;
                        }
                        warning() << "unhandled event" << responseType;
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
    xcb_get_property_cookie_t cookie = xcb_ewmh_get_wm_pid(mEwmhConn, mScreens.at(mPreferredScreenIndex).screen->root);
    uint32_t pid;
    if (!xcb_ewmh_get_wm_pid_reply(mEwmhConn, cookie, &pid, 0))
        return false;

    return pid != static_cast<uint32_t>(getpid()) && !kill(pid, 0);
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

void WindowManager::setRect(const Rect& rect, int idx)
{
    Screen &screen = mScreens[idx];
    screen.rect = rect;
    for (Workspace *ws : screen.workspaces) {
        ws->setRect(rect);
    }
}

void WindowManager::addWorkspace(unsigned int layoutType, int screenNumber)
{
    if (screenNumber == AllScreens) {
        const int count = mScreens.size();
        for (int i=0; i<count; ++i)
            addWorkspace(layoutType, i);
    } else {
        Screen &screen = mScreens[screenNumber];
        screen.workspaces.append(new Workspace(layoutType, screenNumber, screen.rect));
    }
}

void WindowManager::setMoveModifier(const String& mod)
{
    mMoveModifier = mod;
    mMoveModifierMask = Keybinding::modToMask(mod);
}

List<xcb_window_t> WindowManager::roots() const
{
    List<xcb_window_t> roots;
    roots.reserve(mScreens.size());
    for (const auto &it : mScreens) {
        roots.append(it.screen->root);
    }
    return roots;
}

int WindowManager::screenNumber(xcb_window_t root) const
{
    int screenNumber = 0;
    for (const auto &it : mScreens) {
        if (it.screen->root == root)
            return screenNumber;
        ++screenNumber;
    }
    return -1;
}

xcb_visualtype_t* WindowManager::visualForScreen(unsigned int screen) const
{
    assert(screen < static_cast<unsigned int>(mScreens.size()));
    return mScreens[screen].visual;
}

List<xcb_screen_t*> WindowManager::screens() const
{
    List<xcb_screen_t*> ret;
    ret.reserve(mScreens.size());
    for (const auto &it : mScreens)
        ret.append(it.screen);
    return ret;
}

String WindowManager::displayString() const
{
    if (mCurrentScreen <= 0)
        return mDisplay;
    return mDisplay + "." + String::number(mCurrentScreen);
}
void WindowManager::setFocusedClient(const Client::SharedPtr &client)
{
    if (auto shared = mFocused.lock()) {
        if (shared == client)
            return;
        mJS.onClientFocusLost(shared);
    }
    mFocused = client;
    mCurrentScreen = client->screenNumber();
    if (client)
        mJS.onClientFocused(client);
}

std::pair<int16_t, int16_t> WindowManager::pointer(bool *ok) const
{
    xcb_query_pointer_cookie_t cookie = xcb_query_pointer(mConn, roots()[mCurrentScreen]);
    AutoPointer<xcb_generic_error_t> err;
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(mConn, cookie, &err);
    if (err) {
        LOG_ERROR(err, "Unable to query pointer");
        if (ok)
            *ok = false;
        return std::pair<int16_t, int16_t>(0, 0);
    }
    if (ok)
        *ok = true;

    return std::make_pair(reply->root_x, reply->root_y);
}


bool WindowManager::warpPointer(int16_t x, int16_t y, int screen, PointerMode mode)
{
    if (screen == -1) {
        screen = mCurrentScreen;
    } else {
        #warning "what do to here?"
    }
    if (mode == Warp_Absolute) {
        bool ok;
        auto cur = pointer(&ok);
        if (!ok)
            return false;
        x -= cur.first;
        y -= cur.second;
    }
    xcb_void_cookie_t cookie = xcb_warp_pointer_checked(mConn, XCB_NONE, XCB_NONE, 0, 0, 0, 0, x, y);
    AutoPointer<xcb_generic_error_t> err(xcb_request_check(mConn, cookie));
    if (err) {
        LOG_ERROR(err, "Unable to warp pointer");
        return false;
    }

    return true;
}
