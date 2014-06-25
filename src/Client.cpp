#include "Client.h"
#include "WindowManager.h"
#include "Types.h"
#include "Atoms.h"
#include <assert.h>
#include <rct/Log.h>

Hash<xcb_window_t, Client::SharedPtr> Client::sClients;

Client::Client(xcb_window_t win)
    : mWindow(win), mValid(false), mNoFocus(false)
{
    error() << "making client";
    WindowManager::SharedPtr wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    updateState(conn);
    wm->rebindKeys(win);
    error() << "valid client" << mRequestedSize.width << mRequestedSize.height;
    mValid = true;
    mLayout = wm->layout()->add(Size({ mRequestedSize.width, mRequestedSize.height }));
    const Rect& layoutRect = mLayout->rect();
    error() << "laid out at" << layoutRect;
    wm->layout()->dump();
    mLayout->rectChanged().connect(std::bind(&Client::onLayoutChanged, this, std::placeholders::_1));
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
                      layoutRect.x, layoutRect.y, layoutRect.width, layoutRect.height, 0,
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

    {
        uint16_t windowMask = 0;
        uint32_t windowValues[3];
        int i = 0;
        if (mRequestedSize.width != layoutRect.width) {
            windowMask |= XCB_CONFIG_WINDOW_WIDTH;
            windowValues[i++] = layoutRect.width;
        }
        if (mRequestedSize.height != layoutRect.height) {
            windowMask |= XCB_CONFIG_WINDOW_HEIGHT;
            windowValues[i++] = layoutRect.height;
        }
        windowMask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        windowValues[i++] = 0;
        xcb_configure_window(conn, mWindow, windowMask, windowValues);
    }

    const uint32_t stackMode[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(conn, mFrame, XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
#warning do xinerama placement
    const uint32_t stateMode[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, mWindow, Atoms::WM_STATE, Atoms::WM_STATE, 32, 2, stateMode);

    focus();
}

Client::~Client()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_screen_t* screen = WindowManager::instance()->screen();
    if (mWindow)
        xcb_reparent_window(conn, mWindow, screen->root, 0, 0);
    xcb_destroy_window(conn, mFrame);
}

void Client::updateState(xcb_connection_t* conn)
{
    const xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(conn, mWindow);
    const xcb_get_property_cookie_t normalHintsCookie = xcb_icccm_get_wm_normal_hints(conn, mWindow);
    const xcb_get_property_cookie_t transientCookie = xcb_icccm_get_wm_transient_for(conn, mWindow);
    const xcb_get_property_cookie_t hintsCookie = xcb_icccm_get_wm_hints(conn, mWindow);
    const xcb_get_property_cookie_t classCookie = xcb_icccm_get_wm_class(conn, mWindow);
    const xcb_get_property_cookie_t protocolsCookie = xcb_icccm_get_wm_protocols(conn, mWindow, Atoms::WM_PROTOCOLS);

    updateSize(conn, geomCookie);
    updateNormalHints(conn, normalHintsCookie);
    updateTransient(conn, transientCookie);
    updateHints(conn, hintsCookie);
    updateClass(conn, classCookie);
    updateProtocols(conn, protocolsCookie);
}

void Client::updateSize(xcb_connection_t* conn, xcb_get_geometry_cookie_t cookie)
{
    xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(conn, cookie, 0);
    FreeScope freeGeom(geom);
    mRequestedSize.width = geom->width;
    mRequestedSize.height = geom->height;
}

void Client::updateNormalHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (xcb_icccm_get_wm_normal_hints_reply(conn, cookie, &mNormalHints, 0)) {
        // overwrite the response from xcb_get_geometry_unchecked
        if (mNormalHints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE
            || mNormalHints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE) {
            mRequestedSize.width = mNormalHints.width;
            mRequestedSize.height = mNormalHints.height;
        }
    } else {
        memset(&mNormalHints, '\0', sizeof(mNormalHints));
    }
}

void Client::updateTransient(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (!xcb_icccm_get_wm_transient_for_reply(conn, cookie, &mTransientFor, 0)) {
        mTransientFor = XCB_NONE;
    }
}

void Client::updateHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (xcb_icccm_get_wm_hints_reply(conn, cookie, &mWmHints, 0)) {
        if (mWmHints.flags & XCB_ICCCM_WM_HINT_INPUT) {
            mNoFocus = !mWmHints.input;
        }
    } else {
        memset(&mWmHints, '\0', sizeof(mWmHints));
    }
}

void Client::updateClass(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    xcb_icccm_get_wm_class_reply_t prop;
    if (xcb_icccm_get_wm_class_reply(conn, cookie, &prop, 0)) {
        mClass.instanceName = prop.instance_name;
        mClass.className = prop.class_name;
        xcb_icccm_get_wm_class_reply_wipe(&prop);
    } else {
        mClass.instanceName.clear();
        mClass.className.clear();
    }
}

void Client::updateProtocols(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    mProtocols.clear();
    xcb_icccm_get_wm_protocols_reply_t prop;
    if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &prop, 0)) {
        for (uint32_t i = 0; i < prop.atoms_len; ++i) {
            mProtocols.insert(prop.atoms[i]);
        }
        xcb_icccm_get_wm_protocols_reply_wipe(&prop);
    }
}

void Client::onLayoutChanged(const Rect& rect)
{
    error() << "layout changed" << rect;
    xcb_connection_t* conn = WindowManager::instance()->connection();
    {
        const uint16_t mask = XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT;
        const uint32_t values[4] = { rect.x, rect.y, rect.width, rect.height };
        xcb_configure_window(conn, mFrame, mask, values);
    }
    {
        const uint16_t mask = XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT;
        const uint32_t values[2] = { rect.width, rect.height };
        xcb_configure_window(conn, mWindow, mask, values);
    }
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

void Client::focus()
{
    const bool takeFocus = mProtocols.count(Atoms::WM_TAKE_FOCUS) > 0;
    if (mNoFocus && !takeFocus)
        return;
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (takeFocus) {
        xcb_client_message_event_t event;
        memset(&event, '\0', sizeof(event));
        event.response_type = XCB_CLIENT_MESSAGE;
        event.window = mWindow;
        event.format = 32;
        event.type = Atoms::WM_PROTOCOLS;
        event.data.data32[0] = Atoms::WM_TAKE_FOCUS;
        event.data.data32[1] = wm->timestamp();

        xcb_send_event(wm->connection(), false, mWindow, XCB_EVENT_MASK_NO_EVENT,
                       reinterpret_cast<char*>(&event));
    }
    xcb_set_input_focus(wm->connection(), XCB_INPUT_FOCUS_PARENT, mWindow, wm->timestamp());
}

void Client::destroy()
{
    unmap();
    release(mWindow);
    mWindow = 0;
}
