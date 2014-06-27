#ifndef CLIENT_H
#define CLIENT_H

#include "Layout.h"
#include "Rect.h"
#include <rct/Hash.h>
#include <rct/Set.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <memory>

class Workspace;

class Client : public std::enable_shared_from_this<Client>
{
public:
    typedef std::shared_ptr<Client> SharedPtr;
    typedef std::weak_ptr<Client> WeakPtr;

    ~Client();

    static SharedPtr client(xcb_window_t window);
    static SharedPtr manage(xcb_window_t window);
    static void release(xcb_window_t window);
    static void clear() { sClients.clear(); }

    // ### heavy, should be improved
    static List<Client::SharedPtr> clients() { return sClients.values(); }

    void release() { release(mWindow); mWindow = 0; }
    bool isValid() const { return mValid; }

    void map();
    void unmap();
    void focus();
    void destroy();

    bool noFocus() const { return mNoFocus; }

    std::shared_ptr<Workspace> workspace() const { return mWorkspace.lock(); }

    const Layout::SharedPtr& layout() const { return mLayout; }
    xcb_window_t window() const { return mWindow; }

private:
    void clearWorkspace();
    void updateWorkspace(const std::shared_ptr<Workspace>& workspace);

private:
    Client(xcb_window_t win);

    void onLayoutChanged(const Rect& rect);
    void updateState(xcb_connection_t* conn);
    void updateSize(xcb_connection_t* conn, xcb_get_geometry_cookie_t cookie);
    void updateNormalHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateTransient(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateClass(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateProtocols(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);

private:
    xcb_window_t mWindow;
    xcb_window_t mFrame;
    bool mValid;
    bool mNoFocus;
    Layout::SharedPtr mLayout;
    std::weak_ptr<Workspace> mWorkspace;

    Size mRequestedSize;
    xcb_size_hints_t mNormalHints;
    xcb_window_t mTransientFor;
    xcb_icccm_wm_hints_t mWmHints;
    struct {
        String instanceName;
        String className;
    } mClass;
    Set<xcb_atom_t> mProtocols;

    static Hash<xcb_window_t, Client::SharedPtr> sClients;

    friend class Workspace;
};

#endif
