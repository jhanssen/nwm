#ifndef ATOMS_H
#define ATOMS_H

#include <string>
#include <xcb/xcb_atom.h>

namespace Atoms
{
extern xcb_atom_t _NET_SUPPORTED;
extern xcb_atom_t _NET_STARTUP_ID;
extern xcb_atom_t _NET_CLIENT_LIST;
extern xcb_atom_t _NET_CLIENT_LIST_STACKING;
extern xcb_atom_t _NET_NUMBER_OF_DESKTOPS;
extern xcb_atom_t _NET_CURRENT_DESKTOP;
extern xcb_atom_t _NET_DESKTOP_NAMES;
extern xcb_atom_t _NET_ACTIVE_WINDOW;
extern xcb_atom_t _NET_SUPPORTING_WM_CHECK;
extern xcb_atom_t _NET_CLOSE_WINDOW;
extern xcb_atom_t _NET_WM_NAME;
extern xcb_atom_t _NET_WM_STRUT_PARTIAL;
extern xcb_atom_t _NET_WM_VISIBLE_NAME;
extern xcb_atom_t _NET_WM_DESKTOP;
extern xcb_atom_t _NET_WM_ICON_NAME;
extern xcb_atom_t _NET_WM_VISIBLE_ICON_NAME;
extern xcb_atom_t _NET_WM_WINDOW_TYPE;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DESKTOP;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DOCK;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_TOOLBAR;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_MENU;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_UTILITY;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_SPLASH;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DIALOG;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_POPUP_MENU;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_TOOLTIP;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_NOTIFICATION;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_COMBO;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DND;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_NORMAL;
extern xcb_atom_t _NET_WM_ICON;
extern xcb_atom_t _NET_WM_PID;
extern xcb_atom_t _NET_WM_STATE;
extern xcb_atom_t _NET_WM_STATE_STICKY;
extern xcb_atom_t _NET_WM_STATE_SKIP_TASKBAR;
extern xcb_atom_t _NET_WM_STATE_FULLSCREEN;
extern xcb_atom_t _NET_WM_STATE_MAXIMIZED_VERT;
extern xcb_atom_t _NET_WM_STATE_MAXIMIZED_HORZ;
extern xcb_atom_t _NET_WM_STATE_ABOVE;
extern xcb_atom_t _NET_WM_STATE_BELOW;
extern xcb_atom_t _NET_WM_STATE_MODAL;
extern xcb_atom_t _NET_WM_STATE_HIDDEN;
extern xcb_atom_t _NET_WM_STATE_DEMANDS_ATTENTION;
extern xcb_atom_t UTF8_STRING;
extern xcb_atom_t COMPOUND_TEXT;
extern xcb_atom_t WM_PROTOCOLS;
extern xcb_atom_t WM_DELETE_WINDOW;
extern xcb_atom_t _XEMBED;
extern xcb_atom_t _XEMBED_INFO;
extern xcb_atom_t _NET_SYSTEM_TRAY_OPCODE;
extern xcb_atom_t _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR;
extern xcb_atom_t MANAGER;
extern xcb_atom_t _XROOTPMAP_ID;
extern xcb_atom_t ESETROOT_PMAP_ID;
extern xcb_atom_t WM_STATE;
extern xcb_atom_t _NET_WM_WINDOW_OPACITY;
extern xcb_atom_t _NET_SYSTEM_TRAY_ORIENTATION;
extern xcb_atom_t WM_CHANGE_STATE;
extern xcb_atom_t WM_WINDOW_ROLE;
extern xcb_atom_t WM_CLIENT_LEADER;
extern xcb_atom_t XSEL_DATA;
extern xcb_atom_t WM_TAKE_FOCUS;

bool setup(xcb_connection_t* conn);
std::string name(xcb_atom_t atom); // not efficient, call this for debugging only
};

#endif
