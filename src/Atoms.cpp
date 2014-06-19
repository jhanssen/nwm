#include "Atoms.h"

struct Atom
{
    const char* name;
    xcb_atom_t atom;
};

template<int Count>
static void setupAtoms(Atom (&atoms)[Count])
{
    std::vector<xcb_intern_atom_cookie_t> cookies;
    cookies.reserve(Count);
    for (int i = 0; i < Count; ++i) {
        cookies.push_back(std::make_pair(atoms[i].name, xcb_intern_atom(mConn, 0, strlen(atoms[i].name), atoms[i].name)));
    }
    xcb_generic_error_t* err;
    for (int i = 0; i < Count; ++i) {
        if (xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(mConn, cookies[i], &err)) {
            atoms[i].atom = reply->atom;
            free(reply);
        } else {
            atoms[i].atom = 0;
            if (err)
                free(err);
        }
    }
}

bool Atoms::setup()
{
    const Atom atoms[] = {
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
    setupAtoms(atoms);
    return true;
}
