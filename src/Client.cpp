#include "Client.h"
#include "WindowManager.h"
#include "Types.h"
#include "Atoms.h"
#include <assert.h>
#include <rct/Log.h>
#include <sys/types.h>
#include <signal.h>

Hash<xcb_window_t, Client::SharedPtr> Client::sClients;

Client::Client(xcb_window_t win)
    : mWindow(win), mFrame(XCB_NONE), mNoFocus(false), mLayout(0),
      mWorkspace(0), mGraphics(0), mGroup(0), mFloating(false), mPid(0),
      mScreenNumber(0)
{
    warning() << "making client";
}

Client::~Client()
{
    delete mGraphics;
    xcb_connection_t* conn = WindowManager::instance()->connection();
    if (mWindow)
        xcb_reparent_window(conn, mWindow, root(), 0, 0);
    xcb_destroy_window(conn, mFrame);

    if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
        Workspace *workspace = mWorkspace;
        loop->callLater([workspace]() { workspace->updateFocus(); });
    }
    delete mGroup;
}

void Client::init()
{
    WindowManager *wm = WindowManager::instance();
    xcb_ewmh_connection_t* ewmhConn = wm->ewmhConnection();
    updateState(ewmhConn);
    wm->bindings().rebind(mWindow);
}

void Client::complete()
{
    WindowManager *wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    xcb_ewmh_connection_t* ewmhConn = wm->ewmhConnection();
    Rect layoutRect;
    if (mEwmhState.contains(ewmhConn->_NET_WM_STATE_STICKY)) {
        // don't put in layout
#warning support strut windows in layouts (reserved space)
#warning support partial struts
        Rect rect = wm->rect(mScreenNumber);
        if (mStrut.left) {
            if (mRequestedGeom.width != mStrut.left)
                mRequestedGeom.width = mStrut.left;
            mRequestedGeom.x = rect.x;
            rect.x += mRequestedGeom.width;
            rect.width -= mRequestedGeom.width;
        } else if (mStrut.right) {
            if (mRequestedGeom.width != mStrut.right)
                mRequestedGeom.width = mStrut.right;
            mRequestedGeom.x = rect.x + rect.width - mStrut.right;
            rect.width -= mStrut.right;
        } else if (mStrut.top) {
            if (mRequestedGeom.height != mStrut.top)
                mRequestedGeom.height = mStrut.top;
            mRequestedGeom.y = rect.y;
            rect.y += mRequestedGeom.height;
            rect.height -= mRequestedGeom.height;
        } else if (mStrut.bottom) {
            if (mRequestedGeom.height != mStrut.bottom)
                mRequestedGeom.height = mStrut.bottom;
            mRequestedGeom.y = rect.y + rect.height - mStrut.bottom;
            rect.height -= mStrut.bottom;
        }
        wm->setRect(rect, mScreenNumber);
        layoutRect = mRequestedGeom;
        warning() << "fixed at" << layoutRect;
    } else {
        if (shouldLayout()) {
            mLayout = wm->activeWorkspace(mScreenNumber)->layout()->add(Size({ mRequestedGeom.width, mRequestedGeom.height }));
            layoutRect = mLayout->rect();
            warning() << "laid out at" << layoutRect;
            wm->activeWorkspace(mScreenNumber)->layout()->dump();
            mLayout->rectChanged().connect(std::bind(&Client::onLayoutChanged, this, std::placeholders::_1));
        } else {
            layoutRect = mRequestedGeom;
            mFloating = true;
            warning() << "floating at" << layoutRect;
        }
    }
#warning do startup-notification stuff here
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, mWindow);
    xcb_screen_t* scr = screen();
    mFrame = xcb_generate_id(conn);
    const uint32_t values[] = {
        scr->black_pixel,
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
    warning() << "creating frame window" << layoutRect << mRequestedGeom;
    if (mFloating) {
        const Rect &wsRect = wm->activeWorkspace(mScreenNumber)->rect();
        layoutRect.x = std::max<int>(0, (wsRect.width - layoutRect.width) / 2);
        layoutRect.y = std::max<int>(0, (wsRect.height - layoutRect.height) / 2);
    }
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, mFrame, scr->root,
                      layoutRect.x, layoutRect.y, layoutRect.width, layoutRect.height, 0,
                      XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                      XCB_CW_BORDER_PIXEL | XCB_CW_BIT_GRAVITY | XCB_CW_WIN_GRAVITY
                      | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, values);
    {
        ServerGrabScope grabScope(conn);
        const uint32_t noValue[] = { 0 };
        xcb_change_window_attributes(conn, scr->root, XCB_CW_EVENT_MASK, noValue);
        xcb_reparent_window(conn, mWindow, mFrame, 0, 0);
        warning() << "created and mapped parent client for frame" << mFrame << "with window" << mWindow;
        const uint32_t rootEvent[] = { Types::RootEventMask };
        xcb_change_window_attributes(conn, scr->root, XCB_CW_EVENT_MASK, rootEvent);
        xcb_grab_button(conn, false, mWindow, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, scr->root,
                        XCB_NONE, 1, XCB_BUTTON_MASK_ANY);
        const uint32_t windowEvent[] = { Types::ClientInputMask };
        xcb_change_window_attributes(conn, mWindow, XCB_CW_EVENT_MASK, windowEvent);
    }

    {
        uint16_t windowMask = 0;
        uint32_t windowValues[3];
        int i = 0;
        if (mRequestedGeom.width != layoutRect.width) {
            windowMask |= XCB_CONFIG_WINDOW_WIDTH;
            windowValues[i++] = layoutRect.width;
        }
        if (mRequestedGeom.height != layoutRect.height) {
            windowMask |= XCB_CONFIG_WINDOW_HEIGHT;
            windowValues[i++] = layoutRect.height;
        }
        windowMask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        windowValues[i++] = 0;
        xcb_configure_window(conn, mWindow, windowMask, windowValues);
    }

    raise();
#warning do xinerama placement
    const uint32_t stateMode[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, mWindow, Atoms::WM_STATE, Atoms::WM_STATE, 32, 2, stateMode);
}

void Client::clearWorkspace()
{
    mLayout = 0;
    mWorkspace = 0;
}

bool Client::updateWorkspace(Workspace *workspace)
{
    if (!mWorkspace) // ### is this right/
        return false;
    if (workspace == mWorkspace)
        return true;
    mWorkspace->removeClient(shared_from_this());
    if (shouldLayout()) {
        mLayout = workspace->layout()->add(Size({ mRequestedGeom.width, mRequestedGeom.height }));
        mLayout->rectChanged().connect(std::bind(&Client::onLayoutChanged, this, std::placeholders::_1));
    } else {
        assert(mFloating);
    }
    mWorkspace = workspace;

    onLayoutChanged(mLayout->rect());
    return true;
}

void Client::updateState(xcb_ewmh_connection_t* ewmhConn)
{
    xcb_connection_t* conn = ewmhConn->connection;
    const xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry_unchecked(conn, mWindow);
    const xcb_get_property_cookie_t normalHintsCookie = xcb_icccm_get_wm_normal_hints(conn, mWindow);
    const xcb_get_property_cookie_t leaderCookie = xcb_get_property(conn, 0, mWindow, Atoms::WM_CLIENT_LEADER, XCB_ATOM_WINDOW, 0, 1);
    const xcb_get_property_cookie_t transientCookie = xcb_icccm_get_wm_transient_for(conn, mWindow);
    const xcb_get_property_cookie_t hintsCookie = xcb_icccm_get_wm_hints(conn, mWindow);
    const xcb_get_property_cookie_t classCookie = xcb_icccm_get_wm_class(conn, mWindow);
    const xcb_get_property_cookie_t nameCookie = xcb_icccm_get_wm_name(conn, mWindow);
    const xcb_get_property_cookie_t protocolsCookie = xcb_icccm_get_wm_protocols(conn, mWindow, Atoms::WM_PROTOCOLS);
    const xcb_get_property_cookie_t strutCookie = xcb_ewmh_get_wm_strut(ewmhConn, mWindow);
    const xcb_get_property_cookie_t partialStrutCookie = xcb_ewmh_get_wm_strut_partial(ewmhConn, mWindow);
    const xcb_get_property_cookie_t stateCookie = xcb_ewmh_get_wm_state(ewmhConn, mWindow);
    const xcb_get_property_cookie_t typeCookie = xcb_ewmh_get_wm_window_type(ewmhConn, mWindow);
    const xcb_get_property_cookie_t pidCookie = xcb_ewmh_get_wm_pid(ewmhConn, mWindow);

    updateSize(conn, geomCookie);
    updateNormalHints(conn, normalHintsCookie);
    updateLeader(conn, leaderCookie);
    updateTransient(conn, transientCookie);
    updateHints(conn, hintsCookie);
    updateClass(conn, classCookie);
    updateName(conn, nameCookie);
    updateProtocols(conn, protocolsCookie);
    updateStrut(ewmhConn, strutCookie);
    updatePartialStrut(ewmhConn, partialStrutCookie);
    updateEwmhState(ewmhConn, stateCookie);
    updateWindowType(ewmhConn, typeCookie);
    updatePid(ewmhConn, pidCookie);
}

void Client::updateLeader(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    AutoPointer<xcb_get_property_reply_t> leader = xcb_get_property_reply(conn, cookie, 0);
    if (!leader || leader->type != XCB_ATOM_WINDOW || leader->format != 32 || !leader->length) {
        mGroup = ClientGroup::clientGroup(mWindow);
        mGroup->add(shared_from_this());
        return;
    }

    const xcb_window_t win = *static_cast<xcb_window_t *>(xcb_get_property_value(leader));
    mGroup = ClientGroup::clientGroup(win);
    mGroup->add(shared_from_this());
}

void Client::updateSize(xcb_connection_t* conn, xcb_get_geometry_cookie_t cookie)
{
    AutoPointer<xcb_get_geometry_reply_t> geom = xcb_get_geometry_reply(conn, cookie, 0);
    mRequestedGeom = { static_cast<uint32_t>(geom->x), static_cast<uint32_t>(geom->y), geom->width, geom->height };
}

void Client::updateNormalHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (xcb_icccm_get_wm_normal_hints_reply(conn, cookie, &mNormalHints, 0)) {
        // overwrite the response from xcb_get_geometry_unchecked
        if (mNormalHints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE
            || mNormalHints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE) {
            mRequestedGeom.width = mNormalHints.width;
            mRequestedGeom.height = mNormalHints.height;
        }
        if (mNormalHints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION) {
            mRequestedGeom.x = mNormalHints.x;
            mRequestedGeom.y = mNormalHints.y;
        }
    } else {
        memset(&mNormalHints, '\0', sizeof(mNormalHints));
    }
}

void Client::updateTransient(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (!xcb_icccm_get_wm_transient_for_reply(conn, cookie, &mTransientFor, 0)) {
        mTransientFor = XCB_NONE;
    } else {
        if (mTransientFor == XCB_NONE || mTransientFor == root()) {
            // we're really transient for the group
            if (!mGroup) {
                // bad
                error() << "transient-for None or root but no leader set";
                mTransientFor = XCB_NONE;
                return;
            }
            mTransientFor = mGroup->leader();
        } else {
            // add us to the group of the window we're transient for
            Client::SharedPtr other = Client::client(mTransientFor);
            if (!other) {
                error() << "Couldn't find the client we're transient for" << mWindow << mTransientFor;
                mTransientFor = XCB_NONE;
                return;
            }
            ClientGroup *otherGroup = other->group();
            if (mGroup != otherGroup) {
                mGroup = otherGroup;
                mGroup->add(shared_from_this());
            }
        }
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

void Client::updateName(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    xcb_icccm_get_text_property_reply_t prop;
    if (xcb_icccm_get_wm_name_reply(conn, cookie, &prop, 0)) {
        if (prop.format == 8) {
            mName = String(prop.name, prop.name_len);
        } else {
            mName.clear();
        }
        xcb_icccm_get_text_property_reply_wipe(&prop);
    } else {
        mName.clear();
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

void Client::updateStrut(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    memset(&mStrut, '\0', sizeof(mStrut));
    xcb_ewmh_get_extents_reply_t struts;
    if (xcb_ewmh_get_wm_strut_reply(conn, cookie, &struts, 0)) {
        mStrut.left = struts.left;
        mStrut.right = struts.right;
        mStrut.top = struts.top;
        mStrut.bottom = struts.bottom;
        xcb_screen_t *scr = screen();
        mStrut.left_end_y = mStrut.right_end_y = scr->height_in_pixels;
        mStrut.top_end_x = mStrut.bottom_end_x = scr->width_in_pixels;
    }
}

void Client::updatePartialStrut(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (!xcb_ewmh_get_wm_strut_partial_reply(conn, cookie, &mStrut, 0)) {
        memset(&mStrut, '\0', sizeof(mStrut));
    }
}

void Client::updateEwmhState(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    warning() << "updating ewmh state";
    mEwmhState.clear();
    xcb_ewmh_get_atoms_reply_t prop;
    if (xcb_ewmh_get_wm_state_reply(conn, cookie, &prop, 0)) {
        for (uint32_t i = 0; i < prop.atoms_len; ++i) {
            warning() << "ewmh state has" << Atoms::name(prop.atoms[i]);
            mEwmhState.insert(prop.atoms[i]);
        }
        xcb_ewmh_get_atoms_reply_wipe(&prop);
    }
}

void Client::updateWindowType(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    mWindowType.clear();
    xcb_ewmh_get_atoms_reply_t prop;
    if (xcb_ewmh_get_wm_window_type_reply(conn, cookie, &prop, 0)) {
        for (uint32_t i = 0; i < prop.atoms_len; ++i) {
            warning() << "window type has" << Atoms::name(prop.atoms[i]);
            mWindowType.insert(prop.atoms[i]);
        }
        xcb_ewmh_get_atoms_reply_wipe(&prop);
    }

    if (mWindowType.contains(conn->_NET_WM_WINDOW_TYPE_DIALOG) && mTransientFor == XCB_NONE) {
        if (mGroup->leader() != mWindow) {
            mTransientFor = mGroup->leader();
        }
    }
}

void Client::updatePid(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (!xcb_ewmh_get_wm_pid_reply(conn, cookie, &mPid, 0))
        mPid = 0;
}

void Client::onLayoutChanged(const Rect& rect)
{
    warning() << "layout changed" << rect << mClass.className;
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

Client::SharedPtr Client::manage(xcb_window_t window, int screenNumber)
{
    assert(sClients.count(window) == 0);
    Client::SharedPtr ptr(new Client(window)); // can't use make_shared due to private c'tor
    ptr->mScreenNumber = screenNumber;
    ptr->init();
    WindowManager *wm = WindowManager::instance();
    wm->js().onClient(ptr);
    ptr->complete();
    if (ptr->isFloating() || ptr->mLayout) {
        Workspace *ws = wm->activeWorkspace(screenNumber);
        assert(ws);
        ptr->mWorkspace = ws;
        ws->addClient(ptr);
        ptr->focus();
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
        if (it->second->mWorkspace) {
            it->second->mWorkspace->removeClient(it->second);
        }
        WindowManager::instance()->js().onClientDestroyed(it->second);
        sClients.erase(it);
    }
}

void Client::setText(const Rect& rect, const Font& font, const Color& color, const String& string)
{
    if (!mGraphics)
        mGraphics = new Graphics(shared_from_this());
    mGraphics->setText(rect, font, color, string);
    mGraphics->redraw();
}

void Client::clearText()
{
    if (!mGraphics)
        return;
    mGraphics->clearText();
}

void Client::map()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    warning() << "mapping frame" << mFrame;
    xcb_map_window(conn, mWindow);
    xcb_map_window(conn, mFrame);
}

void Client::unmap()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    warning() << "unmapping frame???" << mFrame;
    xcb_unmap_window(conn, mFrame);
    xcb_unmap_window(conn, mWindow);
}

void Client::focus()
{
    const bool takeFocus = mProtocols.count(Atoms::WM_TAKE_FOCUS) > 0;
    if (mNoFocus && !takeFocus)
        return;
    WindowManager *wm = WindowManager::instance();
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
    xcb_ewmh_set_active_window(wm->ewmhConnection(), mScreenNumber, mWindow);
    wm->setFocusedClient(shared_from_this());
    if (mWorkspace)
        mWorkspace->updateFocus(shared_from_this());
}

void Client::destroy()
{
    unmap();
    release(mWindow);
    mWindow = 0;
}

void Client::raise()
{
    warning() << "raising" << this;
    assert(mGroup);
    mGroup->raise(shared_from_this());
}

void Client::resize(const Size& size)
{
    if (!mFloating)
        return;
    mRequestedGeom.width = size.width;
    mRequestedGeom.height = size.height;

    xcb_connection_t* conn = WindowManager::instance()->connection();
    const uint16_t mask = XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[2] = { size.width, size.height };
    xcb_configure_window(conn, mFrame, mask, values);
    xcb_configure_window(conn, mWindow, mask, values);
}

void Client::move(const Point& point)
{
    if (!mFloating)
        return;
    mRequestedGeom.x = point.x;
    mRequestedGeom.y = point.y;

    xcb_connection_t* conn = WindowManager::instance()->connection();
    const uint16_t mask = XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y;
    const uint32_t values[2] = { point.x, point.y };
    xcb_configure_window(conn, mFrame, mask, values);
}

void Client::close()
{
    WindowManager *wm = WindowManager::instance();
    if (mProtocols.contains(Atoms::WM_DELETE_WINDOW)) {
        // delete
        xcb_client_message_event_t event;
        memset(&event, '\0', sizeof(event));
        event.response_type = XCB_CLIENT_MESSAGE;
        event.window = mWindow;
        event.format = 32;
        event.type = Atoms::WM_PROTOCOLS;
        event.data.data32[0] = Atoms::WM_DELETE_WINDOW;
        event.data.data32[1] = wm->timestamp();

        xcb_send_event(wm->connection(), false, mWindow, XCB_EVENT_MASK_NO_EVENT,
                       reinterpret_cast<char*>(&event));

    } else {
        xcb_kill_client(wm->connection(), mWindow);
    }
}

bool Client::kill(int sig)
{
    if (!mPid)
        return false;
    return (::kill(mPid, sig) == 0);
}

void Client::configure()
{
    const Rect& layoutRect = mLayout ? mLayout->rect() : mRequestedGeom;
    xcb_connection_t* conn = WindowManager::instance()->connection();

    xcb_configure_notify_event_t ce;
    ce.response_type = XCB_CONFIGURE_NOTIFY;
    ce.event = mWindow;
    ce.window = mWindow;
    ce.x = layoutRect.x;
    ce.y = layoutRect.y;
    ce.width = layoutRect.width;
    ce.height = layoutRect.height;
    ce.border_width = 0;
    ce.above_sibling = XCB_NONE;
    ce.override_redirect = false;
    xcb_send_event(conn, false, mWindow, XCB_EVENT_MASK_STRUCTURE_NOTIFY, reinterpret_cast<char*>(&ce));
}

bool Client::shouldLayout()
{
    if (mFloating)
        return false;
#warning handle transient-for here
    if (mWindowType.isEmpty())
        return true;
    xcb_ewmh_connection_t* conn = WindowManager::instance()->ewmhConnection();
    return (mWindowType.contains(conn->_NET_WM_WINDOW_TYPE_NORMAL));
}

void Client::expose(const Rect& rect)
{
    if (mGraphics)
        mGraphics->redraw();
}

void Client::createJSValue()
{
    JavaScript& js = WindowManager::instance()->js();
    ScriptEngine::Object::SharedPtr obj = js.clientClass()->create();
    obj->setExtraData(WeakPtr(shared_from_this()));
    mJSValue = js.fromObject(obj);
}

xcb_screen_t *Client::screen() const
{
    return WindowManager::instance()->screens().at(mScreenNumber);
}

xcb_visualtype_t* Client::visual() const
{
    return WindowManager::instance()->visualForScreen(mScreenNumber);
}
