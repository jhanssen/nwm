#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include "Client.h"
#include "JavaScript.h"
#include "Keybindings.h"
#include "Rect.h"
#include "Workspace.h"
#include <rct/List.h>
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <stdlib.h>
#include <rct/SocketServer.h>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xcb_xkb_state_notify_event_t;
struct xcb_xkb_map_notify_event_t;

class WindowManager : public std::enable_shared_from_this<WindowManager>
{
public:
    typedef std::shared_ptr<WindowManager> SharedPtr;
    typedef std::weak_ptr<WindowManager> WeakPtr;

    WindowManager();
    ~WindowManager();

    bool init(int &argc, char **argv);

    Keybindings& bindings() { return mBindings; }

    String displayString() const { return mDisplay; }

    static SharedPtr instance() { return sInstance ? sInstance->shared_from_this() : SharedPtr(); }

    void updateTimestamp(xcb_timestamp_t timestamp) { mTimestamp = timestamp; }
    xcb_timestamp_t timestamp() const { return mTimestamp; }

    xcb_connection_t* connection() const { return mConn; }
    xcb_ewmh_connection_t* ewmhConnection() const { return mEwmhConn; }
    xcb_screen_t* screen() const { return mScreen; }
    int screenNo() const { return mScreenNo; }

    xcb_key_symbols_t* keySymbols() const { return mSyms; }
    int32_t xkbDevice() const { return mXkb.device; }
    xkb_context* xkbContext() const { return mXkb.ctx; }
    xkb_keymap* xkbKeymap() const { return mXkb.keymap; }
    xkb_state* xkbState() const { return mXkb.state; }
    void updateXkbState(xcb_xkb_state_notify_event_t* state);
    void updateXkbMap(xcb_xkb_map_notify_event_t* map);
    String keycodeToString(xcb_keycode_t code);
    xkb_keysym_t keycodeToKeysym(xcb_keycode_t code);

    String moveModifier() const { return mMoveModifier; }
    uint16_t moveModifierMask() const { return mMoveModifierMask; }
    void setMoveModifier(const String& mod);

    void startMoving(const Client::SharedPtr& client, const Point& point) { mMoving = client; mIsMoving = true; mMovingOrigin = point; }
    void stopMoving() { mMoving.reset(); mIsMoving = false; }
    void setMovingOrigin(const Point& point) { mMovingOrigin = point; }
    bool isMoving() const { return mIsMoving; }
    Client::SharedPtr moving() const { return mMoving.lock(); }
    const Point& movingOrigin() const { return mMovingOrigin; }

    enum FocusPolicy { FocusFollowsMouse, FocusClick };
    void setFocusPolicy(FocusPolicy policy) { mFocusPolicy = policy; }
    FocusPolicy focusPolicy() const { return mFocusPolicy; }

    const List<Workspace::SharedPtr>& workspaces() const { return mWorkspaces; }
    void addWorkspace(unsigned int layoutType);

    const Rect& rect() const { return mRect; }
    void setRect(const Rect& rect);

    JavaScript& js() { return mJS; }
    bool shouldRestart() const { return mRestart; }
private:
    bool install();
    bool isRunning();
    bool manage();

private:
    struct Xkb {
        xkb_context* ctx;
        xkb_keymap* keymap;
        xkb_state* state;
        int32_t device;
    } mXkb;

    String mDisplay;

    xcb_connection_t* mConn;
    xcb_ewmh_connection_t* mEwmhConn;
    xcb_screen_t* mScreen;
    int mScreenNo;
    uint8_t mXkbEvent;
    xcb_key_symbols_t* mSyms;
    List<Workspace::SharedPtr> mWorkspaces;
    xcb_timestamp_t mTimestamp;
    Rect mRect;
    JavaScript mJS;
    Keybindings mBindings;
    String mMoveModifier;
    uint16_t mMoveModifierMask;
    Client::WeakPtr mMoving;
    bool mIsMoving;
    Point mMovingOrigin;
    FocusPolicy mFocusPolicy;

    SocketServer mServer;
    bool mRestart;

    static WindowManager *sInstance;
};

class FreeScope
{
public:
    FreeScope(void* d)
        : mD(d)
    {
    }
    ~FreeScope()
    {
        if (mD)
            free(mD);
    }

private:
    void* mD;
};

class ServerGrabScope
{
public:
    ServerGrabScope(xcb_connection_t* conn)
        : mConn(conn)
    {
        xcb_void_cookie_t cookie = xcb_grab_server(mConn);
        if (xcb_request_check(mConn, cookie)) {
            error() << "Unable to grab server";
            abort();
        }
    }
    ~ServerGrabScope()
    {
        xcb_void_cookie_t cookie = xcb_ungrab_server(mConn);
        if (xcb_request_check(mConn, cookie)) {
            error() << "Unable to ungrab server";
            abort();
        }
    }

private:
    xcb_connection_t* mConn;
};

#define LOG_ERROR(err, msg) \
    do { \
        error() << "X error" << err->error_code << msg << "in" << __FUNCTION__ << "@" << __FILE__ << ":" << __LINE__; \
    } while (0)

#endif
