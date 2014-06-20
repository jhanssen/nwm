#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

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

    bool install();

    static SharedPtr instance() { return sInstance; }
    static void release() { sInstance.reset(); }

    xcb_connection_t* connection() const { return mConn; }
    xcb_screen_t* screen() const { return mScreen; }

private:
    xcb_connection_t* mConn;
    xcb_screen_t* mScreen;
    int mScreenNo;

    static SharedPtr sInstance;
};

class XcbScope
{
public:
    XcbScope(void* d)
        : mD(d)
    {
    }
    ~XcbScope()
    {
        if (mD)
            free(mD);
    }

private:
    void* mD;
};

#endif
