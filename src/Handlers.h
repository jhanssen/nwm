#ifndef HANDLERS_H
#define HANDLERS_H

#include <xcb/xcb.h>

namespace Handlers {
void handleButtonPress(const xcb_button_press_event_t* event);
void handleButtonRelease(const xcb_button_release_event_t* event);
void handleMotionNotify(const xcb_motion_notify_event_t* event);
void handleClientMessage(const xcb_client_message_event_t* event);
void handleConfigureRequest(const xcb_configure_request_event_t* event);
void handleConfigureNotify(const xcb_configure_notify_event_t* event);
void handleDestroyNotify(const xcb_destroy_notify_event_t* event);
void handleEnterNotify(const xcb_enter_notify_event_t* event);
void handleExpose(const xcb_expose_event_t* event);
void handleFocusIn(const xcb_focus_in_event_t* event);
void handleKeyPress(const xcb_key_press_event_t* event);
void handleMappingNotify(const xcb_mapping_notify_event_t* event);
void handleMapRequest(const xcb_map_request_event_t* event);
void handlePropertyNotify(const xcb_property_notify_event_t* event);
void handleUnmapNotify(const xcb_unmap_notify_event_t* event);
}; // namespace Handlers

#endif
