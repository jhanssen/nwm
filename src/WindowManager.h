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

class WindowManager
{
public:
    WindowManager();
    ~WindowManager();

    bool init(int &argc, char **argv);

    Keybindings& bindings() { return mBindings; }

    String displayString() const;

    static WindowManager *instance() { return sInstance; }

    void updateTimestamp(xcb_timestamp_t timestamp) { mTimestamp = timestamp; }
    xcb_timestamp_t timestamp() const { return mTimestamp; }

    xcb_connection_t* connection() const { return mConn; }
    xcb_ewmh_connection_t* ewmhConnection() const { return mEwmhConn; }
    List<xcb_screen_t*> screens() const;
    List<xcb_window_t> roots() const;
    enum { AllScreens = -1 };
    int preferredScreenIndex() const { return mPreferredScreenIndex; }
    int screenNumber(xcb_window_t root) const;
    int screenCount() const { return mScreens.size(); }
    const List<Workspace*> & workspaces(int screenNumber) const { return mScreens.at(screenNumber).workspaces; }

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
    Client::SharedPtr focusedClient() const { return mFocused.lock(); }
    void setFocusedClient(const Client::SharedPtr &client) { mFocused = client; mCurrentScreen = client->screenNumber(); }
    void updateCurrentScreen(int screen) { mCurrentScreen = screen; }
    int currentScreen() const { return mCurrentScreen; }
    const Point& movingOrigin() const { return mMovingOrigin; }

    enum FocusPolicy { FocusFollowsMouse, FocusClick };
    void setFocusPolicy(FocusPolicy policy) { mFocusPolicy = policy; }
    FocusPolicy focusPolicy() const { return mFocusPolicy; }

    void addWorkspace(unsigned int layoutType, int screenNumber);

    Rect rect(int idx) const { return mScreens.value(idx).rect; }
    void setRect(const Rect& rect, int idx);

    JavaScript& js() { return mJS; }
    bool shouldRestart() const { return mRestart; }
    Workspace *activeWorkspace(int screenNumber)
    {
        assert(screenNumber >= 0 && screenNumber < mScreens.size());
        return mScreens.at(screenNumber).activeWorkspace;
    }
    void activateWorkspace(Workspace *workspace)
    {
        const int screenNumber = workspace->screenNumber();
        assert(screenNumber >= 0 && screenNumber < mScreens.size());
        mScreens.at(screenNumber).activeWorkspace = workspace;
    }
    void restart()
    {
        mRestart = true;
        EventLoop::eventLoop()->quit();
    }
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
    struct Screen {
        Screen()
            : screen(0), activeWorkspace(0)
        {}

        xcb_screen_t *screen;
        Rect rect;
        List<Workspace*> workspaces;
        Workspace *activeWorkspace;
    };

    List<Screen> mScreens;
    int mPreferredScreenIndex;
    uint8_t mXkbEvent;
    xcb_key_symbols_t* mSyms;
    xcb_timestamp_t mTimestamp;
    JavaScript mJS;
    Keybindings mBindings;
    String mMoveModifier;
    uint16_t mMoveModifierMask;
    Client::WeakPtr mMoving, mFocused;
    bool mIsMoving;
    Point mMovingOrigin;
    FocusPolicy mFocusPolicy;
    int mCurrentScreen;

    SocketServer mServer;
    bool mRestart;

    static WindowManager *sInstance;
};

template <typename T>
class AutoPointer
{
public:
    AutoPointer(T *ptr = 0)
        : mPtr(ptr)
    {
    }
    ~AutoPointer()
    {
        if (mPtr)
            free(mPtr);
    }

    T *operator->() { return mPtr; }
    bool operator!() const { return !mPtr; }
    operator T*() { return mPtr; }
    T **operator&() { return &mPtr; }
    template <typename J> J *cast() { return reinterpret_cast<J*>(mPtr); }
private:
    T *mPtr;
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
