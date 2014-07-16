#ifndef CLIENT_H
#define CLIENT_H

#include "ClientGroup.h"
#include "Graphics.h"
#include "Layout.h"
#include "Rect.h"
#include <rct/Hash.h>
#include <rct/Set.h>
#include <rct/Value.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <memory>

class Workspace;

class Client
{
public:
    ~Client();

    static Client *client(xcb_window_t window);
    static Client *manage(xcb_window_t window, int screenNumber);
    static Client *create(const Rect& rect, int screenNumber,
                          const String &clazz = String(),
                          const String &instance = String());
    static void release(xcb_window_t window);
    static void clear() { sClients.clear(); }

    static Hash<xcb_window_t, Client*> &clients() { return sClients; }

    void setBackgroundColor(const Color& color);
    void setText(const Rect& rect, const Font& font, const Color& color, const String& string);
    void clearText();

    void map();
    void unmap();
    void focus();
    void destroy();
    void configure();
    void raise();
    Point position() const { return mRect.point(); }
    Size size() const { return mRect.size(); }
    void move(const Point& point);
    void resize(const Size& size);
    void close(); // delete or kill
    bool kill(int signal); // kill if we have a _NET_WM_PID
    void expose(const Rect& rect);

    bool noFocus() const { return mNoFocus; }
    bool isFloating() const { return mFloating; }
    bool isDialog() const { return mTransientFor != XCB_NONE; }

    void setFloating(bool floating) { mFloating = floating; }

    const Value& jsValue() { if (mJSValue.type() == Value::Type_Invalid) createJSValue(); return mJSValue; }
    void clearJSValue() { mJSValue.clear(); }

    Workspace* workspace() const { return mWorkspace; }
    ClientGroup *group() const { return mGroup; }

    Layout* layout() const { return mLayout; }
    xcb_window_t window() const { return mWindow; }
    xcb_window_t frame() const { return mFrame; }
    xcb_window_t root() const { return screen()->root; }
    xcb_screen_t* screen() const;
    xcb_visualtype_t* visual() const;
    int screenNumber() const { return mScreenNumber; }

    String wmName() const { return mName; }
    String instanceName() const { return mClass.instanceName; }
    String className() const { return mClass.className; }

    bool isOwned() const { return mOwned; }

    Rect rect() const { return mRect; }
    void setRect(const Rect &rect);
private:
    void clearWorkspace();
    bool updateWorkspace(Workspace* workspace);
    bool shouldLayout();
    void createJSValue();

private:
    Client(xcb_window_t win);
    void init();
    void complete();

    void onLayoutChanged(const Rect& rect);
    void updateState(xcb_ewmh_connection_t* conn);
    void updateSize(xcb_connection_t* conn, xcb_get_geometry_cookie_t cookie);
    void updateNormalHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateTransient(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateHints(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateClass(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateName(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateProtocols(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateStrut(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updatePartialStrut(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateEwmhState(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateWindowType(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateLeader(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updatePid(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);

private:
    xcb_window_t mWindow;
    xcb_window_t mFrame;
    bool mNoFocus, mOwned;
    Layout* mLayout;
    Workspace* mWorkspace;
    Graphics* mGraphics;

    Rect mRect;
    xcb_size_hints_t mNormalHints;
    xcb_window_t mTransientFor;
    xcb_icccm_wm_hints_t mWmHints;
    struct {
        String instanceName;
        String className;
    } mClass;
    String mName;
    Set<xcb_atom_t> mProtocols;
    Set<xcb_atom_t> mEwmhState;
    Set<xcb_atom_t> mWindowType;
    xcb_ewmh_wm_strut_partial_t mStrut;
    ClientGroup *mGroup;
    bool mFloating;
    Value mJSValue;
    uint32_t mPid;
    int mScreenNumber;

    static Hash<xcb_window_t, Client*> sClients;

    friend class Workspace;
};

#endif
