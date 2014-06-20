#include "Handlers.h"
#include "Client.h"
#include "WindowManager.h"

namespace Handlers {

void handleButtonPress(const xcb_button_press_event_t* event)
{
}

void handleClientMessage(const xcb_client_message_event_t* event)
{
}

void handleConfigureRequest(const xcb_configure_request_event_t* event)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();

    Client::SharedPtr client = Client::client(event->window);
    if (client) {
        // stuff
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
}

void handleEnterNotify(const xcb_enter_notify_event_t* event)
{
}

void handleExpose(const xcb_expose_event_t* event)
{
}

void handleFocusIn(const xcb_focus_in_event_t* event)
{
}

void handleKeyPress(const xcb_key_press_event_t* event)
{
}

void handleMappingNotify(const xcb_mapping_notify_event_t* event)
{
}

void handleMapRequest(const xcb_map_request_event_t* event)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    const xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes_unchecked(conn, event->window);
    xcb_get_window_attributes_reply_t* reply = xcb_get_window_attributes_reply(conn, cookie, 0);
    XcbScope scope(reply);
    if (!reply)
        return;
    if (reply->override_redirect) {
        return;
    }
    Client::SharedPtr client = Client::client(event->window);
    if (client) {
        // stuff
    } else {
        client = Client::manage(event->window);
        // more stuff
    }
}

void handlePropertyNotify(const xcb_property_notify_event_t* event)
{
}

void handleUnmapNotify(const xcb_unmap_notify_event_t* event)
{
}

} // namespace Handlers
