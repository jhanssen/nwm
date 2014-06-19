#include "WindowManager.h"
#include "Atoms.h"
#include "Handlers.h"
#include <rct/EventLoop.h>
#include <rct/Rct.h>
#include <rct/Log.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>

WindowManager::SharedPtr WindowManager::sInstance;

WindowManager::WindowManager()
    : mConn(0), mScreen(0), mScreenNo(0)
{
}

WindowManager::~WindowManager()
{
    if (mConn) {
        const int fd = xcb_get_file_descriptor(mConn);
        EventLoop::eventLoop()->unregisterSocket(fd);
        xcb_disconnect(mConn);
        mConn = 0;
    }
}

bool WindowManager::install()
{
    mConn = xcb_connect(0, &mScreenNo);
    if (!mConn)
        return false;
    mScreen = xcb_aux_get_screen(mConn, mScreenNo);
    Atoms::setup(mConn);

    xcb_void_cookie_t cookie;
    xcb_generic_error_t* err;

    // check if another WM is running
    {
        const uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
        cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(mConn, cookie);
        if (err) {
            free(err);
            return false;
        }
    }

    const uint32_t values[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_PROPERTY_CHANGE
    };
    cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
    err = xcb_request_check(mConn, cookie);
    if (err) {
        free(err);
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
        return false;
    }

    const int fd = xcb_get_file_descriptor(mConn);
    EventLoop::eventLoop()->registerSocket(fd, EventLoop::SocketRead, [this](int, unsigned int) {
            for (;;) {
                xcb_generic_event_t* event = xcb_poll_for_event(mConn);
                if (event) {
                    XcbScope scope(event);
                    switch (event->response_type & ~0x80) {
                    case XCB_BUTTON_PRESS:
                        Handlers::handleButtonPress(reinterpret_cast<xcb_button_press_event_t*>(event));
                        break;
                    case XCB_CLIENT_MESSAGE:
                        Handlers::handleClientMessage(reinterpret_cast<xcb_client_message_event_t*>(event));
                        break;
                    case XCB_CONFIGURE_REQUEST:
                        Handlers::handleConfigureRequest(reinterpret_cast<xcb_configure_request_event_t*>(event));
                        break;
                    case XCB_CONFIGURE_NOTIFY:
                        Handlers::handleConfigureNotify(reinterpret_cast<xcb_configure_notify_event_t*>(event));
                        break;
                    case XCB_DESTROY_NOTIFY:
                        Handlers::handleDestroyNotify(reinterpret_cast<xcb_destroy_notify_event_t*>(event));
                        break;
                    case XCB_ENTER_NOTIFY:
                        Handlers::handleEnterNotify(reinterpret_cast<xcb_enter_notify_event_t*>(event));
                        break;
                    case XCB_EXPOSE:
                        Handlers::handleExpose(reinterpret_cast<xcb_expose_event_t*>(event));
                        break;
                    case XCB_FOCUS_IN:
                        Handlers::handleFocusIn(reinterpret_cast<xcb_focus_in_event_t*>(event));
                        break;
                    case XCB_KEY_PRESS:
                        Handlers::handleKeyPress(reinterpret_cast<xcb_key_press_event_t*>(event));
                        break;
                    case XCB_MAPPING_NOTIFY:
                        Handlers::handleMappingNotify(reinterpret_cast<xcb_mapping_notify_event_t*>(event));
                        break;
                    case XCB_MAP_REQUEST:
                        Handlers::handleMapRequest(reinterpret_cast<xcb_map_request_event_t*>(event));
                        break;
                    case XCB_PROPERTY_NOTIFY:
                        Handlers::handlePropertyNotify(reinterpret_cast<xcb_property_notify_event_t*>(event));
                        break;
                    case XCB_UNMAP_NOTIFY:
                        Handlers::handleUnmapNotify(reinterpret_cast<xcb_unmap_notify_event_t*>(event));
                        break;
                    }
                } else {
                    break;
                }
            }
        });

    sInstance = shared_from_this();
    return true;
}
