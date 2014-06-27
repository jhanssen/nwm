#ifndef ATOMS_H
#define ATOMS_H

#include <string>
#include <xcb/xcb_atom.h>

namespace Atoms
{
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
