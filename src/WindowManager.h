#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <memory>
#include <xcb/xcb.h>
#include <rct/Hash.h>

class WindowManager : public std::enable_shared_from_this<WindowManager>
{
public:
    typedef std::shared_ptr<WindowManager> SharedPtr;
    typedef std::weak_ptr<WindowManager> WeakPtr;

    WindowManager();
    ~WindowManager();

    bool install();

private:
    xcb_connection_t* mConn;
    xcb_screen_t* mScreen;
    int mScreenNo;
};

#endif
