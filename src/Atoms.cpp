#include "Atoms.h"
#include "WindowManager.h"
#include <rct/Log.h>
#include <vector>
#include <stdlib.h>
#include <string.h>

struct Atom
{
    const char* name;
    xcb_atom_t& atom;
};

template<int Count>
static void setupAtoms(Atom (&atoms)[Count], xcb_connection_t* conn)
{
    std::vector<xcb_intern_atom_cookie_t> cookies;
    cookies.reserve(Count);
    for (int i = 0; i < Count; ++i) {
        cookies.push_back(xcb_intern_atom(conn, 0, strlen(atoms[i].name), atoms[i].name));
    }
    xcb_generic_error_t* err;
    for (int i = 0; i < Count; ++i) {
        if (xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookies[i], &err)) {
            atoms[i].atom = reply->atom;
            free(reply);
        } else {
            atoms[i].atom = 0;
            if (err)
                free(err);
        }
    }
}

namespace Atoms
{
xcb_atom_t _NET_SUPPORTED;
xcb_atom_t _NET_STARTUP_ID;
xcb_atom_t _NET_CLIENT_LIST;
xcb_atom_t _NET_CLIENT_LIST_STACKING;
xcb_atom_t _NET_NUMBER_OF_DESKTOPS;
xcb_atom_t _NET_CURRENT_DESKTOP;
xcb_atom_t _NET_DESKTOP_NAMES;
xcb_atom_t _NET_ACTIVE_WINDOW;
xcb_atom_t _NET_SUPPORTING_WM_CHECK;
xcb_atom_t _NET_CLOSE_WINDOW;
xcb_atom_t _NET_WM_NAME;
xcb_atom_t _NET_WM_STRUT_PARTIAL;
xcb_atom_t _NET_WM_VISIBLE_NAME;
xcb_atom_t _NET_WM_DESKTOP;
xcb_atom_t _NET_WM_ICON_NAME;
xcb_atom_t _NET_WM_VISIBLE_ICON_NAME;
xcb_atom_t _NET_WM_WINDOW_TYPE;
xcb_atom_t _NET_WM_WINDOW_TYPE_DESKTOP;
xcb_atom_t _NET_WM_WINDOW_TYPE_DOCK;
xcb_atom_t _NET_WM_WINDOW_TYPE_TOOLBAR;
xcb_atom_t _NET_WM_WINDOW_TYPE_MENU;
xcb_atom_t _NET_WM_WINDOW_TYPE_UTILITY;
xcb_atom_t _NET_WM_WINDOW_TYPE_SPLASH;
xcb_atom_t _NET_WM_WINDOW_TYPE_DIALOG;
xcb_atom_t _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
xcb_atom_t _NET_WM_WINDOW_TYPE_POPUP_MENU;
xcb_atom_t _NET_WM_WINDOW_TYPE_TOOLTIP;
xcb_atom_t _NET_WM_WINDOW_TYPE_NOTIFICATION;
xcb_atom_t _NET_WM_WINDOW_TYPE_COMBO;
xcb_atom_t _NET_WM_WINDOW_TYPE_DND;
xcb_atom_t _NET_WM_WINDOW_TYPE_NORMAL;
xcb_atom_t _NET_WM_ICON;
xcb_atom_t _NET_WM_PID;
xcb_atom_t _NET_WM_STATE;
xcb_atom_t _NET_WM_STATE_STICKY;
xcb_atom_t _NET_WM_STATE_SKIP_TASKBAR;
xcb_atom_t _NET_WM_STATE_FULLSCREEN;
xcb_atom_t _NET_WM_STATE_MAXIMIZED_VERT;
xcb_atom_t _NET_WM_STATE_MAXIMIZED_HORZ;
xcb_atom_t _NET_WM_STATE_ABOVE;
xcb_atom_t _NET_WM_STATE_BELOW;
xcb_atom_t _NET_WM_STATE_MODAL;
xcb_atom_t _NET_WM_STATE_HIDDEN;
xcb_atom_t _NET_WM_STATE_DEMANDS_ATTENTION;
xcb_atom_t UTF8_STRING;
xcb_atom_t COMPOUND_TEXT;
xcb_atom_t WM_PROTOCOLS;
xcb_atom_t WM_DELETE_WINDOW;
xcb_atom_t _XEMBED;
xcb_atom_t _XEMBED_INFO;
xcb_atom_t _NET_SYSTEM_TRAY_OPCODE;
xcb_atom_t _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR;
xcb_atom_t MANAGER;
xcb_atom_t _XROOTPMAP_ID;
xcb_atom_t ESETROOT_PMAP_ID;
xcb_atom_t WM_STATE;
xcb_atom_t _NET_WM_WINDOW_OPACITY;
xcb_atom_t _NET_SYSTEM_TRAY_ORIENTATION;
xcb_atom_t WM_CHANGE_STATE;
xcb_atom_t WM_WINDOW_ROLE;
xcb_atom_t WM_CLIENT_LEADER;
xcb_atom_t XSEL_DATA;
xcb_atom_t WM_TAKE_FOCUS;

bool setup(xcb_connection_t* conn)
{
    Atom atoms[] = {
        { "_NET_SUPPORTED", _NET_SUPPORTED },
        { "_NET_STARTUP_ID", _NET_STARTUP_ID },
        { "_NET_CLIENT_LIST", _NET_CLIENT_LIST },
        { "_NET_CLIENT_LIST_STACKING", _NET_CLIENT_LIST_STACKING },
        { "_NET_NUMBER_OF_DESKTOPS", _NET_NUMBER_OF_DESKTOPS },
        { "_NET_CURRENT_DESKTOP", _NET_CURRENT_DESKTOP },
        { "_NET_DESKTOP_NAMES", _NET_DESKTOP_NAMES },
        { "_NET_ACTIVE_WINDOW", _NET_ACTIVE_WINDOW },
        { "_NET_SUPPORTING_WM_CHECK", _NET_SUPPORTING_WM_CHECK },
        { "_NET_CLOSE_WINDOW", _NET_CLOSE_WINDOW },
        { "_NET_WM_NAME", _NET_WM_NAME },
        { "_NET_WM_STRUT_PARTIAL", _NET_WM_STRUT_PARTIAL },
        { "_NET_WM_VISIBLE_NAME", _NET_WM_VISIBLE_NAME },
        { "_NET_WM_DESKTOP", _NET_WM_DESKTOP },
        { "_NET_WM_ICON_NAME", _NET_WM_ICON_NAME },
        { "_NET_WM_VISIBLE_ICON_NAME", _NET_WM_VISIBLE_ICON_NAME },
        { "_NET_WM_WINDOW_TYPE", _NET_WM_WINDOW_TYPE },
        { "_NET_WM_WINDOW_TYPE_DESKTOP", _NET_WM_WINDOW_TYPE_DESKTOP },
        { "_NET_WM_WINDOW_TYPE_DOCK", _NET_WM_WINDOW_TYPE_DOCK },
        { "_NET_WM_WINDOW_TYPE_TOOLBAR", _NET_WM_WINDOW_TYPE_TOOLBAR },
        { "_NET_WM_WINDOW_TYPE_MENU", _NET_WM_WINDOW_TYPE_MENU },
        { "_NET_WM_WINDOW_TYPE_UTILITY", _NET_WM_WINDOW_TYPE_UTILITY },
        { "_NET_WM_WINDOW_TYPE_SPLASH", _NET_WM_WINDOW_TYPE_SPLASH },
        { "_NET_WM_WINDOW_TYPE_DIALOG", _NET_WM_WINDOW_TYPE_DIALOG },
        { "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", _NET_WM_WINDOW_TYPE_DROPDOWN_MENU },
        { "_NET_WM_WINDOW_TYPE_POPUP_MENU", _NET_WM_WINDOW_TYPE_POPUP_MENU },
        { "_NET_WM_WINDOW_TYPE_TOOLTIP", _NET_WM_WINDOW_TYPE_TOOLTIP },
        { "_NET_WM_WINDOW_TYPE_NOTIFICATION", _NET_WM_WINDOW_TYPE_NOTIFICATION },
        { "_NET_WM_WINDOW_TYPE_COMBO", _NET_WM_WINDOW_TYPE_COMBO },
        { "_NET_WM_WINDOW_TYPE_DND", _NET_WM_WINDOW_TYPE_DND },
        { "_NET_WM_WINDOW_TYPE_NORMAL", _NET_WM_WINDOW_TYPE_NORMAL },
        { "_NET_WM_ICON", _NET_WM_ICON },
        { "_NET_WM_PID", _NET_WM_PID },
        { "_NET_WM_STATE", _NET_WM_STATE },
        { "_NET_WM_STATE_STICKY", _NET_WM_STATE_STICKY },
        { "_NET_WM_STATE_SKIP_TASKBAR", _NET_WM_STATE_SKIP_TASKBAR },
        { "_NET_WM_STATE_FULLSCREEN", _NET_WM_STATE_FULLSCREEN },
        { "_NET_WM_STATE_MAXIMIZED_VERT", _NET_WM_STATE_MAXIMIZED_VERT },
        { "_NET_WM_STATE_MAXIMIZED_HORZ", _NET_WM_STATE_MAXIMIZED_HORZ },
        { "_NET_WM_STATE_ABOVE", _NET_WM_STATE_ABOVE },
        { "_NET_WM_STATE_BELOW", _NET_WM_STATE_BELOW },
        { "_NET_WM_STATE_MODAL", _NET_WM_STATE_MODAL },
        { "_NET_WM_STATE_HIDDEN", _NET_WM_STATE_HIDDEN },
        { "_NET_WM_STATE_DEMANDS_ATTENTION", _NET_WM_STATE_DEMANDS_ATTENTION },
        { "UTF8_STRING", UTF8_STRING },
        { "COMPOUND_TEXT", COMPOUND_TEXT },
        { "WM_PROTOCOLS", WM_PROTOCOLS },
        { "WM_DELETE_WINDOW", WM_DELETE_WINDOW },
        { "_XEMBED", _XEMBED },
        { "_XEMBED_INFO", _XEMBED_INFO },
        { "_NET_SYSTEM_TRAY_OPCODE", _NET_SYSTEM_TRAY_OPCODE },
        { "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR },
        { "MANAGER", MANAGER },
        { "_XROOTPMAP_ID", _XROOTPMAP_ID },
        { "ESETROOT_PMAP_ID", ESETROOT_PMAP_ID },
        { "WM_STATE", WM_STATE },
        { "_NET_WM_WINDOW_OPACITY", _NET_WM_WINDOW_OPACITY },
        { "_NET_SYSTEM_TRAY_ORIENTATION", _NET_SYSTEM_TRAY_ORIENTATION },
        { "WM_CHANGE_STATE", WM_CHANGE_STATE },
        { "WM_WINDOW_ROLE", WM_WINDOW_ROLE },
        { "WM_CLIENT_LEADER", WM_CLIENT_LEADER },
        { "XSEL_DATA", XSEL_DATA },
        { "WM_TAKE_FOCUS", WM_TAKE_FOCUS }
    };
    setupAtoms(atoms, conn);
    return true;
}

std::string name(xcb_atom_t atom)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(conn, atom);
    xcb_generic_error_t* err;
    xcb_get_atom_name_reply_t* reply = xcb_get_atom_name_reply(conn, cookie, &err);
    if (err) {
        free(err);
        return std::string();
    }

    std::string name;
    if (reply) {
        const int len = xcb_get_atom_name_name_length(reply);
        name = std::string(xcb_get_atom_name_name(reply), len);
        free(reply);
    }
    return name;
}

} // namespace Atoms
