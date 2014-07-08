#include "Handlers.h"
#include "Atoms.h"
#include "Client.h"
#include "Util.h"
#include "WindowManager.h"
#include <xkbcommon/xkbcommon.h>
#include <rct/Log.h>

namespace Handlers {

void handleButtonPress(const xcb_button_press_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    wm->updateTimestamp(event->time);

    Client::SharedPtr client = Client::client(event->event);
    if (client) {
        xcb_connection_t* conn = wm->connection();
        client->raise();
        if (wm->focusPolicy() == WindowManager::FocusClick)
            client->focus();
        xcb_flush(conn);

        if (event->state) {
            const uint16_t mod = wm->moveModifierMask();
            if (mod && (event->state & mod) == mod && client->isFloating()) {
                const xcb_window_t root = wm->screen()->root;
                // grab both the keyboard and the pointer
                xcb_grab_pointer_cookie_t pointerCookie = xcb_grab_pointer(conn, false, root,
                                                                           XCB_EVENT_MASK_BUTTON_RELEASE
                                                                           | XCB_EVENT_MASK_POINTER_MOTION,
                                                                           XCB_GRAB_MODE_ASYNC,
                                                                           XCB_GRAB_MODE_ASYNC,
                                                                           XCB_NONE, XCB_NONE,
                                                                           XCB_CURRENT_TIME);
                xcb_grab_pointer_reply_t* pointerReply = xcb_grab_pointer_reply(conn, pointerCookie, 0);
                if (!pointerReply) {
                    error() << "Unable to grab pointer for move";
                    xcb_allow_events(conn, XCB_ALLOW_ASYNC_POINTER, event->time);
                    return;
                }
                free(pointerReply);
                xcb_grab_keyboard_cookie_t keyboardCookie = xcb_grab_keyboard(conn, false, root,
                                                                              XCB_CURRENT_TIME,
                                                                              XCB_GRAB_MODE_ASYNC,
                                                                              XCB_GRAB_MODE_ASYNC);
                xcb_grab_keyboard_reply_t* keyboardReply = xcb_grab_keyboard_reply(conn, keyboardCookie, 0);
                if (!keyboardReply) {
                    // bad!
                    error() << "Unable to grab keyboard for move";
                    // ungrab pointer and move on
                    xcb_void_cookie_t ungrabCookie = xcb_ungrab_pointer(conn, event->time);
                    if (xcb_request_check(conn, ungrabCookie)) {
                        // we're done
                        error() << "Unable to ungrab pointer after successfully grabing";
                        abort();
                    }
                    xcb_allow_events(conn, XCB_ALLOW_ASYNC_POINTER, event->time);
                    return;
                }
                free(keyboardReply);
                wm->startMoving(client, Point({ static_cast<unsigned int>(event->root_x),
                                                static_cast<unsigned int>(event->root_y) }));
            }
            xcb_allow_events(conn, XCB_ALLOW_ASYNC_POINTER, event->time);
            return;
        }

        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
    }
}

static inline void releaseGrab(xcb_connection_t* conn, xcb_timestamp_t time)
{
    // ungrab pointer and keyboard
    xcb_void_cookie_t ungrabCookie = xcb_ungrab_pointer(conn, time);
    if (xcb_request_check(conn, ungrabCookie)) {
        // we're done
        error() << "Unable to ungrab pointer after successfully grabing (release)";
        abort();
    }
    ungrabCookie = xcb_ungrab_keyboard(conn, time);
    if (xcb_request_check(conn, ungrabCookie)) {
        // we're done
        error() << "Unable to ungrab keyboard after successfully grabing (release)";
        abort();
    }
}

void handleButtonRelease(const xcb_button_release_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    wm->updateTimestamp(event->time);
    if (!wm->isMoving())
        return;
    wm->stopMoving();
    releaseGrab(wm->connection(), event->time);
}

void handleMotionNotify(const xcb_motion_notify_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    wm->updateTimestamp(event->time);
    if (!wm->isMoving())
        return;

    Client::SharedPtr client = wm->moving();
    Point origin = wm->movingOrigin();
    if (client) {
        // move window
        Point point = Point({ static_cast<unsigned int>(event->root_x),
                              static_cast<unsigned int>(event->root_y) });
        Point current = client->position();
        current.x += (point.x - origin.x);
        current.y += (point.y - origin.y);
        client->move(current);
        wm->setMovingOrigin(point);
    } else {
        error() << "Client gone while moving";
        wm->stopMoving();
        releaseGrab(wm->connection(), event->time);
    }
}

void handleClientMessage(const xcb_client_message_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    xcb_ewmh_connection_t* ewmhConn = wm->ewmhConnection();
    if (event->type == ewmhConn->_NET_ACTIVE_WINDOW) {
        Client::SharedPtr client = Client::client(event->window);
        if (client) {
            client->raise();
            client->focus();
        }
    } else if (event->type == ewmhConn->_NET_CURRENT_DESKTOP) {
        const uint32_t ws = event->data.data32[0];
        const List<Workspace::SharedPtr>& wss = wm->workspaces();
        if (ws >= static_cast<uint32_t>(wss.size()))
            return;
        wss[ws]->activate();
        xcb_ewmh_set_current_desktop(ewmhConn, wm->screenNo(), ws);
    }
}

void handleConfigureRequest(const xcb_configure_request_event_t* event)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();

    Client::SharedPtr client = Client::client(event->window);
    if (client) {
#warning need to restack here

        // I know better than you, go away
        client->configure();
    } else {
        // configure
        uint16_t windowMask = 0;
        uint32_t windowValues[7];
        int i = 0;

        if(event->value_mask & XCB_CONFIG_WINDOW_X) {
            windowMask |= XCB_CONFIG_WINDOW_X;
            windowValues[i++] = event->x;
        }
        if(event->value_mask & XCB_CONFIG_WINDOW_Y) {
            windowMask |= XCB_CONFIG_WINDOW_Y;
            windowValues[i++] = event->y;
        }
        if(event->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            windowMask |= XCB_CONFIG_WINDOW_WIDTH;
            windowValues[i++] = event->width;
        }
        if(event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            windowMask |= XCB_CONFIG_WINDOW_HEIGHT;
            windowValues[i++] = event->height;
        }
        if(event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            windowMask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
            windowValues[i++] = event->border_width;
        }
        if(event->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
            windowMask |= XCB_CONFIG_WINDOW_SIBLING;
            windowValues[i++] = event->sibling;
        }
        if(event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            windowMask |= XCB_CONFIG_WINDOW_STACK_MODE;
            windowValues[i++] = event->stack_mode;
        }

        xcb_configure_window(conn, event->window, windowMask, windowValues);
    }
}

void handleConfigureNotify(const xcb_configure_notify_event_t* event)
{
}

void handleDestroyNotify(const xcb_destroy_notify_event_t* event)
{
    Client::SharedPtr client = Client::client(event->window);
    if (client) {
        client->destroy();
    }
}

void handleEnterNotify(const xcb_enter_notify_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    wm->updateTimestamp(event->time);
    if (wm->focusPolicy() == WindowManager::FocusFollowsMouse) {
        Client::SharedPtr client = Client::client(event->child);
        if (client)
            client->focus();
    }
}

void handleExpose(const xcb_expose_event_t* event)
{
}

void handleFocusIn(const xcb_focus_in_event_t* event)
{
}

void handleKeyPress(const xcb_key_press_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    wm->updateTimestamp(event->time);
    //const int col = (event->state & XCB_MOD_MASK_SHIFT);
    const int col = 0;
    const xcb_keysym_t sym = xcb_key_press_lookup_keysym(wm->keySymbols(), const_cast<xcb_key_press_event_t*>(event), col);
    if (xcb_is_modifier_key(sym))
        return;
    Keybindings& bindings = wm->bindings();
    if (!bindings.feed(sym, event->state))
        return;
    const Keybinding* binding = bindings.current();
    assert(binding);
    const Value& func = binding->function();
    assert(func.type() == Value::Type_Custom);
    JavaScript& engine = wm->js();
    std::shared_ptr<ScriptEngine::Object> obj = engine.toObject(func);
    if (!obj) {
        error() << "couldn't get object for key bind" << wm->keycodeToString(event->detail);
        return;
    }
    assert(obj);
    obj->call();
}

void handleMappingNotify(const xcb_mapping_notify_event_t* event)
{
}

void handleMapRequest(const xcb_map_request_event_t* event)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    const xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes_unchecked(conn, event->window);
    xcb_get_window_attributes_reply_t* reply = xcb_get_window_attributes_reply(conn, cookie, 0);
    FreeScope scope(reply);
    if (!reply)
        return;
    if (reply->override_redirect) {
        error() << "override_redirect";
        return;
    }
    error() << "managing?";
    Client::SharedPtr client = Client::client(event->window);
    if (client) {
        // stuff
    } else {
        client = Client::manage(event->window);
        // more stuff
    }
    client->map();
}

void handlePropertyNotify(const xcb_property_notify_event_t* event)
{
    WindowManager::instance()->updateTimestamp(event->time);
    warning() << "notifying for property" << Atoms::name(event->atom);
#warning Propagate properties to client
}

void handleUnmapNotify(const xcb_unmap_notify_event_t* event)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (wm->isMoving()) {
        error() << "client unmapped while moving, releasing grab";
        wm->stopMoving();
        releaseGrab(wm->connection(), wm->timestamp());
    }

    error() << "unmapping" << event->event << event->window;
    Client::SharedPtr client = Client::client(event->event);
    if (client) {
        client->unmap();
    }
}

} // namespace Handlers
