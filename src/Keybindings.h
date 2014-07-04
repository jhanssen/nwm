#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include "Keybinding.h"
#include <rct/List.h>
#include <rct/Set.h>
#include <memory>
#include <xcb/xcb.h>

class Keybindings
{
public:
    Keybindings();
    ~Keybindings();

    bool feed(xkb_keysym_t sym, uint16_t mods);
    const Keybinding* current();

    void add(const Keybinding& binding);

    void rebindAll();
    void rebind(xcb_window_t win);

private:
    void rebind(const Keybinding& binding, xcb_connection_t* conn, xcb_window_t win);

private:
    Set<Keybinding> mKeybindings;
    Set<Keybinding::Sequence> mPrefixes;

    Keybinding mFeed;
    bool mGrabbed;
};

#endif
