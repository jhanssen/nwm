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
            if (err) {
                LOG_ERROR(err, "Couldn't query atom");
                free(err);
            }
        }
    }
}

namespace Atoms
{
xcb_atom_t COMPOUND_TEXT;
xcb_atom_t ESETROOT_PMAP_ID;
xcb_atom_t MANAGER;
xcb_atom_t UTF8_STRING;
xcb_atom_t WM_CHANGE_STATE;
xcb_atom_t WM_CLIENT_LEADER;
xcb_atom_t WM_DELETE_WINDOW;
xcb_atom_t WM_PROTOCOLS;
xcb_atom_t WM_STATE;
xcb_atom_t WM_TAKE_FOCUS;
xcb_atom_t WM_WINDOW_ROLE;
xcb_atom_t XSEL_DATA;
xcb_atom_t _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR;
xcb_atom_t _NET_SYSTEM_TRAY_OPCODE;
xcb_atom_t _NET_SYSTEM_TRAY_ORIENTATION;
xcb_atom_t _NET_WM_WINDOW_OPACITY;
xcb_atom_t _NWM_PID;
xcb_atom_t _XEMBED;
xcb_atom_t _XEMBED_INFO;
xcb_atom_t _XROOTPMAP_ID;

bool setup(xcb_connection_t* conn)
{
    Atom atoms[] = {
        { "COMPOUND_TEXT", COMPOUND_TEXT },
        { "ESETROOT_PMAP_ID", ESETROOT_PMAP_ID },
        { "MANAGER", MANAGER },
        { "UTF8_STRING", UTF8_STRING },
        { "WM_CHANGE_STATE", WM_CHANGE_STATE },
        { "WM_CLIENT_LEADER", WM_CLIENT_LEADER },
        { "WM_DELETE_WINDOW", WM_DELETE_WINDOW },
        { "WM_PROTOCOLS", WM_PROTOCOLS },
        { "WM_STATE", WM_STATE },
        { "WM_TAKE_FOCUS", WM_TAKE_FOCUS },
        { "WM_WINDOW_ROLE", WM_WINDOW_ROLE },
        { "XSEL_DATA", XSEL_DATA },
        { "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR },
        { "_NWM_PID", _NWM_PID },
        { "_NET_SYSTEM_TRAY_OPCODE", _NET_SYSTEM_TRAY_OPCODE },
        { "_NET_SYSTEM_TRAY_ORIENTATION", _NET_SYSTEM_TRAY_ORIENTATION },
        { "_NET_WM_WINDOW_OPACITY", _NET_WM_WINDOW_OPACITY },
        { "_XEMBED", _XEMBED },
        { "_XEMBED_INFO", _XEMBED_INFO },
        { "_XROOTPMAP_ID", _XROOTPMAP_ID }
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
        LOG_ERROR(err, "Couldn't get atom name");
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
