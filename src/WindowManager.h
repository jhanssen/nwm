#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include "Client.h"
#include "JavaScript.h"
#include "Keybinding.h"
#include "Rect.h"
#include "Workspace.h"
#include <rct/List.h>
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <stdlib.h>

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

    void addKeybinding(const Keybinding& binding);

    String displayString() const { return mDisplay; }

    static SharedPtr instance() { return sInstance; }
    static void release();

    void updateTimestamp(xcb_timestamp_t timestamp) { mTimestamp = timestamp; }
    xcb_timestamp_t timestamp() const { return mTimestamp; }

    xcb_connection_t* connection() const { return mConn; }
    xcb_ewmh_connection_t* ewmhConnection() const { return mEwmhConn; }
    xcb_screen_t* screen() const { return mScreen; }

    xcb_key_symbols_t* keySymbols() const { return mSyms; }
    int32_t xkbDevice() const { return mXkb.device; }
    xkb_context* xkbContext() const { return mXkb.ctx; }
    xkb_keymap* xkbKeymap() const { return mXkb.keymap; }
    xkb_state* xkbState() const { return mXkb.state; }
    void updateXkbState(xcb_xkb_state_notify_event_t* state);
    void updateXkbMap(xcb_xkb_map_notify_event_t* map);
    String keycodeToString(xcb_keycode_t code);
    xkb_keysym_t keycodeToKeysym(xcb_keycode_t code);

    void rebindKeys(xcb_window_t win);
    const Keybinding* lookupKeybinding(xkb_keysym_t sym, uint16_t mods);

    const List<Workspace::SharedPtr>& workspaces() const { return mWorkspaces; }
    void setWorkspaceCount(int count);

    const Rect& rect() const { return mRect; }
    void setRect(const Rect& rect);

    JavaScript& js() { return mJS; }
private:
    bool install();
    void rebindKeys();

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
    List<Keybinding> mKeybindings;
    List<Workspace::SharedPtr> mWorkspaces;
    xcb_timestamp_t mTimestamp;
    Rect mRect;
    JavaScript mJS;

    static SharedPtr sInstance;
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
        xcb_grab_server(mConn);
    }
    ~ServerGrabScope()
    {
        xcb_ungrab_server(mConn);
    }

private:
    xcb_connection_t* mConn;
};

#define LOG_ERROR(err, msg) \
    do { \
        error() << "X error" << err->error_code << msg << "in" << __FUNCTION__ << "@" << __FILE__ << ":" << __LINE__; \
    } while (0)

#endif
