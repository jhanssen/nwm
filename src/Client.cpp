#include "Client.h"
#include "WindowManager.h"
#include "Types.h"
#include "Atoms.h"
#include <assert.h>
#include <rct/Log.h>
#include <sys/types.h>
#include <signal.h>

Hash<xcb_window_t, Client*> Client::sClients;

Client::Client(xcb_window_t win)
    : mWindow(win), mFrame(XCB_NONE), mNoFocus(false), mOwned(false),
      mMovable(false), mWorkspace(0), mGraphics(0), mPid(0), mScreenNumber(0)
{
    warning() << "making client";
}

Client::~Client()
{
    if (mWorkspace)
        mWorkspace->onClientDestroyed(this);
    WindowManager::instance()->js().onClientDestroyed(this);
    assert(mJSValue.isInvalid() || mJSValue.isCustom());
    if (!mJSValue.isInvalid()) {
        JavaScript &engine = WindowManager::instance()->js();
        ScriptEngine::Object::SharedPtr obj = engine.toObject(mJSValue);
        if (obj)
            obj->setExtraData(0);
    }
    unmap();
    delete mGraphics;
    xcb_connection_t* conn = WindowManager::instance()->connection();
    if (mWindow) {
        if (!mOwned) {
            xcb_reparent_window(conn, mWindow, root(), 0, 0);
        } else {
            xcb_destroy_window(conn, mWindow);
        }
        sClients.erase(mWindow);
    }
    xcb_destroy_window(conn, mFrame);
    if (mGroup)
        mGroup->onClientDestroyed(this);
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
    if (mEwmhState.contains(ewmhConn->_NET_WM_STATE_STICKY)) {
        // don't put in layout
#warning support strut windows in layouts (reserved space)
#warning support partial struts
        Rect rect = wm->rect(mScreenNumber);
        if (mStrut.left) {
            if (mRect.width != static_cast<int>(mStrut.left))
                mRect.width = mStrut.left;
            mRect.x = rect.x;
            rect.x += mRect.width;
            rect.width -= mRect.width;
        } else if (mStrut.right) {
            if (mRect.width != static_cast<int>(mStrut.right))
                mRect.width = mStrut.right;
            mRect.x = rect.x + rect.width - mStrut.right;
            rect.width -= mStrut.right;
        } else if (mStrut.top) {
            if (mRect.height != static_cast<int>(mStrut.top))
                mRect.height = mStrut.top;
            mRect.y = rect.y;
            rect.y += mRect.height;
            rect.height -= mRect.height;
        } else if (mStrut.bottom) {
            if (mRect.height != static_cast<int>(mStrut.bottom))
                mRect.height = mStrut.bottom;
            mRect.y = rect.y + rect.height - mStrut.bottom;
            rect.height -= mStrut.bottom;
        }
        wm->setRect(rect, mScreenNumber);
        warning() << "fixed at" << mRect;
    } else {
        if (shouldLayout()) {
            wm->js().onLayout(this);
            warning() << "laid out at" << mRect;
        }
    }
#warning do startup-notification stuff here
    if (!mOwned)
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
    warning() << "creating frame window" << mRect;
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, mFrame, scr->root,
                      mRect.x, mRect.y, mRect.width, mRect.height, 0,
                      XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                      XCB_CW_BORDER_PIXEL | XCB_CW_BIT_GRAVITY | XCB_CW_WIN_GRAVITY
                      | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, values);
    {
        ServerGrabScope grabScope(conn);
        const uint32_t noValue[] = { 0 };
        xcb_change_window_attributes(conn, scr->root, XCB_CW_EVENT_MASK, noValue);
        xcb_reparent_window(conn, mWindow, mFrame, 0, 0);
        const uint32_t rootEvent[] = { Types::RootEventMask };
        xcb_change_window_attributes(conn, scr->root, XCB_CW_EVENT_MASK, rootEvent);
        xcb_grab_button(conn, false, mWindow, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, scr->root,
                        XCB_NONE, 1, XCB_BUTTON_MASK_ANY);
        const uint32_t windowEvent[] = { Types::ClientInputMask };
        xcb_change_window_attributes(conn, mWindow, XCB_CW_EVENT_MASK, windowEvent);
    }

    {
        uint16_t windowMask = XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT|XCB_CONFIG_WINDOW_BORDER_WIDTH;
        uint32_t windowValues[3];
        int i = 0;
        windowValues[i++] = mRect.width;
        windowValues[i++] = mRect.height;
        windowValues[i++] = 0;
        xcb_configure_window(conn, mWindow, windowMask, windowValues);
    }

#warning do xinerama placement
    const uint32_t stateMode[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, mWindow, Atoms::WM_STATE, Atoms::WM_STATE, 32, 2, stateMode);

    map();
    raise();
    warning() << "created and mapped parent client for frame" << mFrame << "with window" << mWindow;
}

void Client::clearWorkspace()
{
    mWorkspace = 0;
}

bool Client::updateWorkspace(Workspace *workspace)
{
    if (!mWorkspace) // ### is this right/
        return false;
    if (workspace == mWorkspace)
        return true;
    mWorkspace->removeClient(this);
    mWorkspace = workspace;
    if (shouldLayout()) {
        WindowManager::instance()->js().onLayout(this);
    }

    return true;
}

void Client::updateState(xcb_ewmh_connection_t* ewmhConn)
{
    xcb_connection_t* conn = ewmhConn->connection;
    xcb_get_geometry_cookie_t geomCookie;
    if (!mOwned)
        geomCookie = xcb_get_geometry_unchecked(conn, mWindow);
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

    if (!mOwned)
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
    updateWindowTypes(ewmhConn, typeCookie);
    updatePid(ewmhConn, pidCookie);
}

void Client::updateLeader(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    AutoPointer<xcb_get_property_reply_t> leader(xcb_get_property_reply(conn, cookie, 0));
    if (!leader || leader->type != XCB_ATOM_WINDOW || leader->format != 32 || !leader->length) {
        mGroup = ClientGroup::clientGroup(mWindow);
        mGroup->add(this);
        return;
    }

    const xcb_window_t win = *static_cast<xcb_window_t *>(xcb_get_property_value(leader));
    mGroup = ClientGroup::clientGroup(win);
    mGroup->add(this);
}

void Client::updateSize(xcb_connection_t* conn, xcb_get_geometry_cookie_t cookie)
{
    AutoPointer<xcb_get_geometry_reply_t> geom(xcb_get_geometry_reply(conn, cookie, 0));
    mRect = Rect(geom->x, geom->y, geom->width, geom->height);
}

void Client::updateNormalHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    if (xcb_icccm_get_wm_normal_hints_reply(conn, cookie, &mNormalHints, 0)) {
        // overwrite the response from xcb_get_geometry_unchecked
        enum { SIZE = (XCB_ICCCM_SIZE_HINT_US_SIZE|XCB_ICCCM_SIZE_HINT_P_SIZE) };
        if ((mNormalHints.flags & SIZE) == SIZE) {
            mRect.width = mNormalHints.width;
            mRect.height = mNormalHints.height;
        }
        enum { POS = (XCB_ICCCM_SIZE_HINT_P_POSITION|XCB_ICCCM_SIZE_HINT_US_POSITION) };
        if ((mNormalHints.flags & POS) == POS) {
            mRect.x = mNormalHints.x;
            mRect.y = mNormalHints.y;
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
            Client *other = Client::client(mTransientFor);
            if (!other) {
                error() << "Couldn't find the client we're transient for" << mWindow << mTransientFor;
                mTransientFor = XCB_NONE;
                return;
            }
            ClientGroup *otherGroup = other->group();
            if (mGroup != otherGroup) {
                mGroup = otherGroup;
                mGroup->add(this);
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

void Client::updateWindowTypes(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie)
{
    mWindowTypes.clear();
    xcb_ewmh_get_atoms_reply_t prop;
    if (xcb_ewmh_get_wm_window_type_reply(conn, cookie, &prop, 0)) {
        for (uint32_t i = 0; i < prop.atoms_len; ++i) {
            warning() << "window type has" << Atoms::name(prop.atoms[i]);
            mWindowTypes.append(prop.atoms[i]);
        }
        xcb_ewmh_get_atoms_reply_wipe(&prop);
    }

    if (mWindowTypes.contains(conn->_NET_WM_WINDOW_TYPE_DIALOG) && mTransientFor == XCB_NONE) {
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

Client *Client::create(const Rect& rect, int screenNumber, const String &clazz, const String &instance, bool movable)
{
    WindowManager *wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    xcb_screen_t* scr = wm->screens().at(screenNumber);

    xcb_window_t window = xcb_generate_id(conn);
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
    warning() << "creating client window" << rect;
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, window, scr->root,
                      rect.x, rect.y, rect.width, rect.height, 0,
                      XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                      XCB_CW_BORDER_PIXEL | XCB_CW_BIT_GRAVITY | XCB_CW_WIN_GRAVITY
                      | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, values);

    xcb_icccm_wm_hints_t wmHints;
    xcb_icccm_wm_hints_set_none(&wmHints);
    xcb_icccm_wm_hints_set_input(&wmHints, 0);
    xcb_icccm_set_wm_hints(conn, window, &wmHints);

    xcb_size_hints_t wmNormalHints;
    memset(&wmNormalHints, 0, sizeof(wmNormalHints));
    xcb_icccm_size_hints_set_position(&wmNormalHints, 1, rect.x, rect.y);
    xcb_icccm_size_hints_set_size(&wmNormalHints, 1, rect.width, rect.height);
    xcb_icccm_set_wm_normal_hints(conn, window, &wmNormalHints);

    String className = clazz + ' ' + instance;
    className[clazz.size()] = '\0';
    xcb_icccm_set_wm_class(conn, window, className.size(), className.constData());
    Client *ptr = new Client(window);
    ptr->mMovable = movable;
    ptr->mRect = rect;
    ptr->mOwned = true;
    ptr->mScreenNumber = screenNumber;
    ptr->init();
    ptr->mNoFocus = true;
    Workspace *ws = wm->activeWorkspace(screenNumber);
    assert(ws);
    ptr->mWorkspace = ws;
    ws->addClient(ptr);

    wm->js().onClient(ptr);
    ptr->complete();

    sClients[window] = ptr;
    return ptr;
}

Client *Client::manage(xcb_window_t window, int screenNumber)
{
    assert(sClients.count(window) == 0);
    Client *ptr = new Client(window);
    ptr->mScreenNumber = screenNumber;
    ptr->init();
    WindowManager *wm = WindowManager::instance();
    wm->js().onClient(ptr);

    xcb_ewmh_connection_t* ewmhConn = wm->ewmhConnection();
    bool focus = false;
    if (!ptr->mEwmhState.contains(ewmhConn->_NET_WM_STATE_STICKY)) {
        Workspace *ws = wm->activeWorkspace(screenNumber);
        assert(ws);
        ptr->mWorkspace = ws;
        ws->addClient(ptr);
        focus = true;
    }
    ptr->complete();
    if (focus)
        ptr->focus();

    sClients[window] = ptr;
    return ptr;
}

Client *Client::client(xcb_window_t window)
{
    return sClients.value(window);
}

Client *Client::clientByFrame(xcb_window_t frame)
{
    for (const auto it : sClients) {
        if (it.second->mFrame == frame)
            return it.second;
    }
    return 0;
}

void Client::setBackgroundColor(const Color& color)
{
    // We don't want to draw on clients we don't own
    if (!mOwned)
        return;
    if (!mGraphics)
        mGraphics = new Graphics(this);
    mGraphics->setBackgroundColor(color);
    mGraphics->redraw();
}

void Client::setText(const Rect& rect, const Font& font, const Color& color, const String& string)
{
    // We don't want to draw on clients we don't own
    if (!mOwned)
        return;
    if (!mGraphics)
        mGraphics = new Graphics(this);
    mGraphics->setText(rect.isEmpty() ? Rect({ 0, 0, mRect.width, mRect.height }) : rect, font, color, string);
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
    if (!mFrame)
        return;
    xcb_connection_t* conn = WindowManager::instance()->connection();
    warning() << "mapping frame" << mFrame;
    xcb_map_window(conn, mWindow);
    xcb_map_window(conn, mFrame);
}

void Client::unmap()
{
    if (!mFrame)
        return;
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
    //error() << "Setting input focus to client" << mWindow << mClass.className;
    xcb_ewmh_set_active_window(wm->ewmhConnection(), mScreenNumber, mWindow);
    wm->setFocusedClient(this);
    if (mWorkspace)
        mWorkspace->updateFocus(this);
}

void Client::restack(xcb_stack_mode_t stackMode, Client *sibling)
{
    warning() << "raising" << this;
    assert(mGroup);
    mGroup->restack(this, stackMode, sibling);
}

void Client::resize(const Size& size)
{
    warning() << "resizing" << size << this;
    mRect.width = size.width;
    mRect.height = size.height;
    if (!mFrame)
        return;

    xcb_connection_t* conn = WindowManager::instance()->connection();
    const uint16_t mask = XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[2] = { static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height) };
    xcb_configure_window(conn, mFrame, mask, values);
    xcb_configure_window(conn, mWindow, mask, values);
}

void Client::move(const Point& point)
{
    warning() << "move" << point << this;
    mRect.x = point.x;
    mRect.y = point.y;
    if (!mFrame)
        return;

    xcb_connection_t* conn = WindowManager::instance()->connection();
    const uint16_t mask = XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y;
    const uint32_t values[2] = { static_cast<uint32_t>(point.x), static_cast<uint32_t>(point.y) };
    xcb_configure_window(conn, mFrame, mask, values);
}

void Client::close()
{
    WindowManager *wm = WindowManager::instance();
    if (mOwned) {
        EventLoop::eventLoop()->callLater([this, wm] {
                delete this;
                xcb_flush(wm->connection());
            });
    } else {
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
}

bool Client::kill(int sig)
{
    if (!mPid)
        return false;
    return (::kill(mPid, sig) == 0);
}

void Client::configure()
{
    xcb_connection_t* conn = WindowManager::instance()->connection();

    xcb_configure_notify_event_t ce;
    ce.response_type = XCB_CONFIGURE_NOTIFY;
    ce.event = mWindow;
    ce.window = mWindow;
    ce.x = mRect.x;
    ce.y = mRect.y;
    ce.width = mRect.width;
    ce.height = mRect.height;
    ce.border_width = 0;
    ce.above_sibling = XCB_NONE;
    ce.override_redirect = false;
    xcb_send_event(conn, false, mWindow, XCB_EVENT_MASK_STRUCTURE_NOTIFY, reinterpret_cast<char*>(&ce));
}

bool Client::shouldLayout()
{
#warning handle transient-for here
    xcb_ewmh_connection_t* conn = WindowManager::instance()->ewmhConnection();
    return windowType() == conn->_NET_WM_WINDOW_TYPE_NORMAL;
}

bool Client::isFloating() const
{
    xcb_ewmh_connection_t* conn = WindowManager::instance()->ewmhConnection();
    return windowType() != conn->_NET_WM_WINDOW_TYPE_NORMAL;
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
    obj->setExtraData(this);
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

void Client::setRect(const Rect &rect)
{
    mRect = rect;

    xcb_connection_t* conn = WindowManager::instance()->connection();
    const uint16_t mask = (XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y
                           |XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT);
    const uint32_t values[4] = { static_cast<uint32_t>(rect.x), static_cast<uint32_t>(rect.y),
                                 static_cast<uint32_t>(rect.width), static_cast<uint32_t>(rect.height) };
    xcb_configure_window(conn, mFrame, mask, values);
}

void Client::propertyNotify(xcb_atom_t atom)
{
#warning Need to notify js that properties have changed
    warning() << "Got propertyNotify" << Atoms::name(atom) << mWindow;
    auto ewmhConnection = WindowManager::instance()->ewmhConnection();
    auto conn = ewmhConnection->connection;
    if (atom == XCB_ATOM_WM_NORMAL_HINTS) {
        const xcb_get_property_cookie_t normalHintsCookie = xcb_icccm_get_wm_normal_hints(conn, mWindow);
        updateNormalHints(conn, normalHintsCookie);
    } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {
        const xcb_get_property_cookie_t transientCookie = xcb_icccm_get_wm_transient_for(conn, mWindow);
        updateTransient(conn, transientCookie);
    } else if (atom == Atoms::WM_CLIENT_LEADER) {
        const xcb_get_property_cookie_t leaderCookie = xcb_get_property(conn, 0, mWindow, Atoms::WM_CLIENT_LEADER, XCB_ATOM_WINDOW, 0, 1);
        updateLeader(conn, leaderCookie);
    } else if (atom == XCB_ATOM_WM_HINTS) {
        const xcb_get_property_cookie_t hintsCookie = xcb_icccm_get_wm_hints(conn, mWindow);
        updateHints(conn, hintsCookie);
    } else if (atom == XCB_ATOM_WM_CLASS) {
        const xcb_get_property_cookie_t classCookie = xcb_icccm_get_wm_class(conn, mWindow);
        updateClass(conn, classCookie);
    } else if (atom == XCB_ATOM_WM_NAME) {
        const xcb_get_property_cookie_t nameCookie = xcb_icccm_get_wm_name(conn, mWindow);
        updateName(conn, nameCookie);
    } else if (atom == Atoms::WM_PROTOCOLS) {
        const xcb_get_property_cookie_t protocolsCookie = xcb_icccm_get_wm_protocols(conn, mWindow, Atoms::WM_PROTOCOLS);
        updateProtocols(conn, protocolsCookie);
    } else if (atom == ewmhConnection->_NET_WM_STRUT) {
        const xcb_get_property_cookie_t strutCookie = xcb_ewmh_get_wm_strut(ewmhConnection, mWindow);
        updateStrut(ewmhConnection, strutCookie);
    } else if (atom == ewmhConnection->_NET_WM_STRUT_PARTIAL) {
        const xcb_get_property_cookie_t partialStrutCookie = xcb_ewmh_get_wm_strut_partial(ewmhConnection, mWindow);
        updatePartialStrut(ewmhConnection, partialStrutCookie);
    } else if (atom == ewmhConnection->_NET_WM_STATE) {
        const xcb_get_property_cookie_t stateCookie = xcb_ewmh_get_wm_state(ewmhConnection, mWindow);
        updateEwmhState(ewmhConnection, stateCookie);
    } else if (atom == ewmhConnection->_NET_WM_WINDOW_TYPE) {
        const xcb_get_property_cookie_t typeCookie = xcb_ewmh_get_wm_window_type(ewmhConnection, mWindow);
        updateWindowTypes(ewmhConnection, typeCookie);
    } else if (atom == ewmhConnection->_NET_WM_PID) {
        const xcb_get_property_cookie_t pidCookie = xcb_ewmh_get_wm_pid(ewmhConnection, mWindow);
        updatePid(ewmhConnection, pidCookie);
    } else {
        warning() << "Unhandled propertyNotify atom" << Atoms::name(atom);
    }
}

xcb_atom_t Client::windowType() const
{
    xcb_ewmh_connection_t* ewmhConn = WindowManager::instance()->ewmhConnection();
    if (mWindowTypes.isEmpty())
        return ewmhConn->_NET_WM_WINDOW_TYPE_NORMAL;

    for (auto type : mWindowTypes) {
        if (type == ewmhConn->_NET_WM_WINDOW_TYPE_DESKTOP
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_DOCK
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_TOOLBAR
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_MENU
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_UTILITY
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_SPLASH
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_DIALOG
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_POPUP_MENU
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_TOOLTIP
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_NOTIFICATION
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_COMBO
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_DND
            || type == ewmhConn->_NET_WM_WINDOW_TYPE_NORMAL)
            return type;
    }
    return XCB_ATOM_NONE;
}
