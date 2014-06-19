#include "Client.h"
#include "WindowManager.h"
#include <assert.h>

Hash<xcb_window_t, Client::SharedPtr> Client::sClients;

Client::Client(xcb_window_t win)
    : mWindow(win), mValid(false)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    const xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(conn, win);
    xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(conn, cookie, 0);
    XcbScope scope(geom);
    if (!geom)
        return;
    mValid = true;
#warning do startup-notification stuff here
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, win);
    xcb_screen_t* screen = WindowManager::instance()->screen();
}

Client::~Client()
{
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
