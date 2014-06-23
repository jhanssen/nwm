#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include "Layout.h"
#include "Keybinding.h"
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
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

    bool install(const char* display = 0);
    void addKeybinding(const Keybinding& binding);

    static SharedPtr instance() { return sInstance; }
    static void release();

    xcb_connection_t* connection() const { return mConn; }
    xcb_screen_t* screen() const { return mScreen; }
    const Layout::SharedPtr& layout() const { return mLayout; }

    int32_t xkbDevice() const { return mXkb.device; }
    xkb_context* xkbContext() const { return mXkb.ctx; }
    xkb_keymap* xkbKeymap() const { return mXkb.keymap; }
    xkb_state* xkbState() const { return mXkb.state; }
    void updateXkbState(xcb_xkb_state_notify_event_t* state);
    void updateXkbMap(xcb_xkb_map_notify_event_t* map);

private:
    void rebindKeys();
    void unbindKeys();

private:
    struct Xkb {
        xkb_context* ctx;
        xkb_keymap* keymap;
        xkb_state* state;
        int32_t device;
    } mXkb;

    xcb_connection_t* mConn;
    xcb_screen_t* mScreen;
    int mScreenNo;
    uint8_t mXkbEvent;
    xcb_key_symbols_t* mSyms;
    Layout::SharedPtr mLayout;

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
