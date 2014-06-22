#include "Client.h"
#include "WindowManager.h"
#include "Types.h"
#include "Atoms.h"
#include <assert.h>
#include <xcb/xcb_icccm.h>
#include <rct/Log.h>

Hash<xcb_window_t, Client::SharedPtr> Client::sClients;

Client::Client(xcb_window_t win)
    : mWindow(win), mValid(false)
{
    error() << "making client";
    WindowManager::SharedPtr wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    const xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(conn, win);
    xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(conn, cookie, 0);
    FreeScope scope(geom);
    if (!geom)
        return;
    error() << "valid client";
    mValid = true;
    mLayout = wm->layout()->add(Size({ geom->width, geom->height }));
    error() << "laid out at" << mLayout->rect();
    wm->layout()->dump();
#warning do startup-notification stuff here
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, win);
    xcb_screen_t* screen = WindowManager::instance()->screen();
    mFrame = xcb_generate_id(conn);
    const uint32_t values[] = {
        screen->black_pixel,
        XCB_GRAVITY_NORTH_WEST,
        XCB_GRAVITY_NORTH_WEST,
        1,
        (XCB_EVENT_MASK_STRUCTURE_NOTIFY
         | XCB_EVENT_MASK_ENTER_WINDOW
         | XCB_EVENT_MASK_LEAVE_WINDOW
         | XCB_EVENT_MASK_EXPOSURE
         | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
         | XCB_EVENT_MASK_POINTER_MOTION
         | XCB_EVENT_MASK_BUTTON_PRESS
         | XCB_EVENT_MASK_BUTTON_RELEASE)
    };
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, mFrame, screen->root,
                      geom->x, geom->y, geom->width, geom->height, geom->border_width,
                      XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                      XCB_CW_BORDER_PIXEL | XCB_CW_BIT_GRAVITY | XCB_CW_WIN_GRAVITY
                      | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, values);
    xcb_grab_server(conn);
    const uint32_t noValue[] = { 0 };
    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, noValue);
    xcb_reparent_window(conn, mWindow, mFrame, 0, 0);
    xcb_map_window(conn, mWindow);
    const uint32_t rootEvent[] = { Types::RootEventMask };
    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK, rootEvent);
    xcb_ungrab_server(conn);
    const uint32_t windowEvent[] = { Types::ClientInputMask };
    xcb_change_window_attributes(conn, mWindow, XCB_CW_EVENT_MASK, windowEvent);
    xcb_configure_window(conn, mWindow, XCB_CONFIG_WINDOW_BORDER_WIDTH, noValue);
    const uint32_t stackMode[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(conn, mFrame, XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
#warning do xinerama placement
    const uint32_t stateMode[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, mWindow, Atoms::WM_STATE, Atoms::WM_STATE, 32, 2, stateMode);
}

Client::~Client()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_screen_t* screen = WindowManager::instance()->screen();
    if (mWindow)
        xcb_reparent_window(conn, mWindow, screen->root, 0, 0);
    xcb_destroy_window(conn, mFrame);
}

Client::SharedPtr Client::manage(xcb_window_t window)
{
    assert(sClients.count(window) == 0);
    Client::SharedPtr ptr(new Client(window)); // can't use make_shared due to private c'tor
    if (!ptr->isValid()) {
        return SharedPtr();
    }
    sClients[window] = ptr;
    return ptr;
}

Client::SharedPtr Client::client(xcb_window_t window)
{
    const Hash<xcb_window_t, Client::SharedPtr>::const_iterator it = sClients.find(window);
    if (it != sClients.end()) {
        return it->second;
    }
    return SharedPtr();
}

void Client::release(xcb_window_t window)
{
    Hash<xcb_window_t, Client::SharedPtr>::iterator it = sClients.find(window);
    if (it != sClients.end()) {
        sClients.erase(it);
    }
}

void Client::map()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_map_window(conn, mFrame);
}

void Client::unmap()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_unmap_window(conn, mFrame);
}

void Client::destroy()
{
    unmap();
    mWindow = 0;
}
