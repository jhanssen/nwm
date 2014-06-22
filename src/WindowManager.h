#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include "Layout.h"
#include <memory>
#include <xcb/xcb.h>
#include <stdlib.h>

class WindowManager : public std::enable_shared_from_this<WindowManager>
{
public:
    typedef std::shared_ptr<WindowManager> SharedPtr;
    typedef std::weak_ptr<WindowManager> WeakPtr;

    WindowManager();
    ~WindowManager();

    bool install(const char* display = 0);

    static SharedPtr instance() { return sInstance; }
    static void release();

    xcb_connection_t* connection() const { return mConn; }
    xcb_screen_t* screen() const { return mScreen; }
    const Layout::SharedPtr& layout() const { return mLayout; }

private:
    xcb_connection_t* mConn;
    xcb_screen_t* mScreen;
    int mScreenNo;
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
