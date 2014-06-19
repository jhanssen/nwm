#include "WindowManager.h"
#include <rct/Rct.h>
#include <xcb/xcb_atoms.h>

WindowManager::WindowManager()
    : mConn(0), mScreen(0), mScreenNo(0)
{
}

WindowManager::WindowManager()
{
    if (mConn) {
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

    // check if another WM is running
    const uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
    xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(mConn, mScreen->root, XCB_CW_EVENT_MASK, values);
    xcb_generic_error_t* err = xcb_request_check(mConn, cookie);
    if (err)
        return false;

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
    xcb_generic_error_t* err = xcb_request_check(mConn, cookie);
    if (err)
        return false;

    xcb_atom_t atom[] = {
        _NET_SUPPORTED,
        _NET_SUPPORTING_WM_CHECK,
        _NET_STARTUP_ID,
        _NET_CLIENT_LIST,
        _NET_CLIENT_LIST_STACKING,
        _NET_NUMBER_OF_DESKTOPS,
        _NET_CURRENT_DESKTOP,
        _NET_DESKTOP_NAMES,
        _NET_ACTIVE_WINDOW,
        _NET_CLOSE_WINDOW,
        _NET_WM_NAME,
        _NET_WM_STRUT_PARTIAL,
        _NET_WM_ICON_NAME,
        _NET_WM_VISIBLE_ICON_NAME,
        _NET_WM_DESKTOP,
        _NET_WM_WINDOW_TYPE,
        _NET_WM_WINDOW_TYPE_DESKTOP,
        _NET_WM_WINDOW_TYPE_DOCK,
        _NET_WM_WINDOW_TYPE_TOOLBAR,
        _NET_WM_WINDOW_TYPE_MENU,
        _NET_WM_WINDOW_TYPE_UTILITY,
        _NET_WM_WINDOW_TYPE_SPLASH,
        _NET_WM_WINDOW_TYPE_DIALOG,
        _NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        _NET_WM_WINDOW_TYPE_POPUP_MENU,
        _NET_WM_WINDOW_TYPE_TOOLTIP,
        _NET_WM_WINDOW_TYPE_NOTIFICATION,
        _NET_WM_WINDOW_TYPE_COMBO,
        _NET_WM_WINDOW_TYPE_DND,
        _NET_WM_WINDOW_TYPE_NORMAL,
        _NET_WM_ICON,
        _NET_WM_PID,
        _NET_WM_STATE,
        _NET_WM_STATE_STICKY,
        _NET_WM_STATE_SKIP_TASKBAR,
        _NET_WM_STATE_FULLSCREEN,
        _NET_WM_STATE_MAXIMIZED_HORZ,
        _NET_WM_STATE_MAXIMIZED_VERT,
        _NET_WM_STATE_ABOVE,
        _NET_WM_STATE_BELOW,
        _NET_WM_STATE_MODAL,
        _NET_WM_STATE_HIDDEN,
        _NET_WM_STATE_DEMANDS_ATTENTION
    };

    xcb_change_property(mConn, XCB_PROP_MODE_REPLACE,
                        mScreen->root, _NET_SUPPORTED, XCB_ATOM_ATOM, 32,
                        Rct::countof(atom), atom);

    return true;
}
