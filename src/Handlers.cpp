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
    WindowManager::instance()->updateTimestamp(event->time);
}

void handleClientMessage(const xcb_client_message_event_t* event)
{
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
    WindowManager::instance()->updateTimestamp(event->time);
    Client::SharedPtr client = Client::client(event->child);
    if (client)
        client->focus();
}

void handleExpose(const xcb_expose_event_t* event)
{
}

void handleFocusIn(const xcb_focus_in_event_t* event)
{
}

void handleKeyPress(const xcb_key_press_event_t* event)
{
    /*
    const char* mods[] = {
        "Shift", "Lock", "Ctrl", "Alt",
        "Mod2", "Mod3", "Mod4", "Mod5",
        "Button1", "Button2", "Button3", "Button4", "Button5"
    };

    uint32_t mask = event->state;
    List<String> modifiers;
    for (const char **mod = mods; mask; mask >>= 1, ++mod) {
        if (mask & 1) {
            modifiers.append(*mod);
        }
    }

    const String key = WindowManager::instance()->keycodeToString(event->detail);
    error() << "got key" << String::join(modifiers, "-") << key;
    error() << "sym" << WindowManager::instance()->keycodeToKeysym(event->detail);
    */
    WindowManager::SharedPtr wm = WindowManager::instance();
    wm->updateTimestamp(event->time);
    //const int col = (event->state & XCB_MOD_MASK_SHIFT);
    const int col = 0;
    const xcb_keysym_t sym = xcb_key_press_lookup_keysym(wm->keySymbols(), const_cast<xcb_key_press_event_t*>(event), col);
    //const xkb_keysym_t sym = wm->keycodeToKeysym(event->detail);
    const Keybinding* binding = wm->lookupKeybinding(sym, event->state);
    if (!binding) {
        char buf[256];
        xkb_keysym_get_name(sym, buf, sizeof(buf));
        error() << "no keybind for" << sym << buf;
        return;
    }
    const Value& func = binding->function();
    if (func.type() == Value::Type_Custom) {
        JavaScript& engine = wm->js();
        std::shared_ptr<ScriptEngine::Object> obj = engine.toObject(func);
        assert(obj);
        obj->call();
    }
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
    error() << "notifying for property" << Atoms::name(event->atom);
}

void handleUnmapNotify(const xcb_unmap_notify_event_t* event)
{
    Client::SharedPtr client = Client::client(event->window);
    if (client)
        client->unmap();
}

} // namespace Handlers
