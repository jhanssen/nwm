#include "WindowManager.h"
#include "Atoms.h"
#include "Client.h"
#include "Handlers.h"
#include "Types.h"
#include <rct/EventLoop.h>
#include <rct/Rct.h>
#include <rct/Log.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>

WindowManager::SharedPtr WindowManager::sInstance;

WindowManager::WindowManager()
    : mConn(0), mScreen(0), mScreenNo(0)
{
}

WindowManager::~WindowManager()
{
    Client::clear();
    if (mConn) {
        const int fd = xcb_get_file_descriptor(mConn);
        EventLoop::eventLoop()->unregisterSocket(fd);
        xcb_disconnect(mConn);
        mConn = 0;
    }
}

bool WindowManager::install(const char* display)
{
    sInstance = shared_from_this();

    mConn = xcb_connect(display, &mScreenNo);
    if (!mConn) {
        sInstance.reset();
        return false;
    }

    mScreen = xcb_aux_get_screen(mConn, mScreenNo);
    Atoms::setup(mConn);

    xcb_void_cookie_t cookie;
    xcb_generic_error_t* err;

    GrabScope scope(mConn);

    // check if another WM is running
    {
        const uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
        cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(mConn, cookie);
        if (err) {
            free(err);
            sInstance.reset();
            return false;
        }
    }

    const uint32_t values[] = { Types::RootEventMask };
    cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
    err = xcb_request_check(mConn, cookie);
    if (err) {
        free(err);
        sInstance.reset();
        return false;
    }

    const xcb_atom_t atom[] = {
        Atoms::_NET_SUPPORTED,
        Atoms::_NET_SUPPORTING_WM_CHECK,
        Atoms::_NET_STARTUP_ID,
        Atoms::_NET_CLIENT_LIST,
        Atoms::_NET_CLIENT_LIST_STACKING,
        Atoms::_NET_NUMBER_OF_DESKTOPS,
        Atoms::_NET_CURRENT_DESKTOP,
        Atoms::_NET_DESKTOP_NAMES,
        Atoms::_NET_ACTIVE_WINDOW,
        Atoms::_NET_CLOSE_WINDOW,
        Atoms::_NET_WM_NAME,
        Atoms::_NET_WM_STRUT_PARTIAL,
        Atoms::_NET_WM_ICON_NAME,
        Atoms::_NET_WM_VISIBLE_ICON_NAME,
        Atoms::_NET_WM_DESKTOP,
        Atoms::_NET_WM_WINDOW_TYPE,
        Atoms::_NET_WM_WINDOW_TYPE_DESKTOP,
        Atoms::_NET_WM_WINDOW_TYPE_DOCK,
        Atoms::_NET_WM_WINDOW_TYPE_TOOLBAR,
        Atoms::_NET_WM_WINDOW_TYPE_MENU,
        Atoms::_NET_WM_WINDOW_TYPE_UTILITY,
        Atoms::_NET_WM_WINDOW_TYPE_SPLASH,
        Atoms::_NET_WM_WINDOW_TYPE_DIALOG,
        Atoms::_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        Atoms::_NET_WM_WINDOW_TYPE_POPUP_MENU,
        Atoms::_NET_WM_WINDOW_TYPE_TOOLTIP,
        Atoms::_NET_WM_WINDOW_TYPE_NOTIFICATION,
        Atoms::_NET_WM_WINDOW_TYPE_COMBO,
        Atoms::_NET_WM_WINDOW_TYPE_DND,
        Atoms::_NET_WM_WINDOW_TYPE_NORMAL,
        Atoms::_NET_WM_ICON,
        Atoms::_NET_WM_PID,
        Atoms::_NET_WM_STATE,
        Atoms::_NET_WM_STATE_STICKY,
        Atoms::_NET_WM_STATE_SKIP_TASKBAR,
        Atoms::_NET_WM_STATE_FULLSCREEN,
        Atoms::_NET_WM_STATE_MAXIMIZED_HORZ,
        Atoms::_NET_WM_STATE_MAXIMIZED_VERT,
        Atoms::_NET_WM_STATE_ABOVE,
        Atoms::_NET_WM_STATE_BELOW,
        Atoms::_NET_WM_STATE_MODAL,
        Atoms::_NET_WM_STATE_HIDDEN,
        Atoms::_NET_WM_STATE_DEMANDS_ATTENTION
    };

    cookie = xcb_change_property(mConn, XCB_PROP_MODE_REPLACE,
                                 mScreen->root, Atoms::_NET_SUPPORTED, XCB_ATOM_ATOM, 32,
                                 Rct::countof(atom), atom);
    err = xcb_request_check(mConn, cookie);
    if (err) {
        free(err);
        sInstance.reset();
        return false;
    }

    // Manage all existing windows
    {
        xcb_query_tree_cookie_t treeCookie = xcb_query_tree(mConn, mScreen->root);
        xcb_query_tree_reply_t* treeReply = xcb_query_tree_reply(mConn, treeCookie, &err);
        FreeScope scope(treeReply);
        if (err) {
            free(err);
            sInstance.reset();
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
                client->map();
            }
        }
    }

    // Get events
    const int fd = xcb_get_file_descriptor(mConn);
    EventLoop::eventLoop()->registerSocket(fd, EventLoop::SocketRead, [this](int, unsigned int) {
            for (;;) {
                xcb_generic_event_t* event = xcb_poll_for_event(mConn);
                if (event) {
                    FreeScope scope(event);
                    switch (event->response_type & ~0x80) {
                    case XCB_BUTTON_PRESS:
                        error() << "button press";
                        Handlers::handleButtonPress(reinterpret_cast<xcb_button_press_event_t*>(event));
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
                    case XCB_MAPPING_NOTIFY:
                        error() << "mapping notify";
                        Handlers::handleMappingNotify(reinterpret_cast<xcb_mapping_notify_event_t*>(event));
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
                        error() << "unhandled event" << (event->response_type & ~0x80);
                        break;
                    }
                } else {
                    break;
                }
            }
            xcb_flush(mConn);
        });

    return true;
}
