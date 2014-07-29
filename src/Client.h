#ifndef CLIENT_H
#define CLIENT_H

#include "ClientGroup.h"
#include "Graphics.h"
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
    static Client *clientByFrame(xcb_window_t frame);

    static Client *manage(xcb_window_t window, int screenNumber);
    static Client *create(const Rect& rect,
                          int screenNumber,
                          const String &clazz,
                          const String &instance,
                          bool movable);
    static void clear() { sClients.clear(); }

    static Hash<xcb_window_t, Client*> &clients() { return sClients; }

    void setBackgroundColor(const Color& color);
    void setText(const Rect& rect, const Font& font, const Color& color, const String& string);
    void clearText();

    void map();
    void unmap();
    void focus();
    void configure();
    void restack(xcb_stack_mode_t stackMode, Client *sibling = 0);
    void raise() { restack(XCB_STACK_MODE_ABOVE); }
    Point position() const { return mRect.point(); }
    Size size() const { return mRect.size(); }
    void move(const Point& point);
    void resize(const Size& size);
    void close(); // delete or kill
    bool kill(int signal); // kill if we have a _NET_WM_PID
    void expose(const Rect& rect);

    bool noFocus() const { return mNoFocus; }
    bool isDialog() const { return mTransientFor != XCB_NONE; }

    bool hasUserSpecifiedPosition() const { return mNormalHints.flags & (XCB_ICCCM_SIZE_HINT_P_POSITION|XCB_ICCCM_SIZE_HINT_US_POSITION); }
    bool hasUserSpecifiedSize() const { return mNormalHints.flags & (XCB_ICCCM_SIZE_HINT_US_SIZE|XCB_ICCCM_SIZE_HINT_P_SIZE); }

    bool isFloating() const;

    const Value& jsValue() { if (mJSValue.type() == Value::Type_Invalid) createJSValue(); return mJSValue; }
    void clearJSValue() { mJSValue.clear(); }

    Workspace* workspace() const { return mWorkspace; }
    ClientGroup *group() const { return mGroup; }

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

    bool isMovable() const { return mMovable || isFloating(); }
    void setMovable(bool movable) { error() << "movable set to" << movable << "for" << mClass.className; mMovable = movable; }

    Rect rect() const { return mRect; }
    void setRect(const Rect &rect);

    void propertyNotify(xcb_atom_t atom);
    xcb_atom_t windowType() const;
private:
    void clearWorkspace();
    bool updateWorkspace(Workspace* workspace);
    bool shouldLayout();
    void createJSValue();

private:
    Client(xcb_window_t win);
    void init();
    void complete();

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
    void updateWindowTypes(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updateLeader(xcb_connection_t* conn, xcb_get_property_cookie_t cookie);
    void updatePid(xcb_ewmh_connection_t* conn, xcb_get_property_cookie_t cookie);

private:
    xcb_window_t mWindow;
    xcb_window_t mFrame;
    bool mNoFocus, mOwned, mMovable;
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
    List<xcb_atom_t> mWindowTypes;
    xcb_ewmh_wm_strut_partial_t mStrut;
    ClientGroup *mGroup;
    Value mJSValue;
    uint32_t mPid;
    int mScreenNumber;

    static Hash<xcb_window_t, Client*> sClients;

    friend class Workspace;
};

#endif
