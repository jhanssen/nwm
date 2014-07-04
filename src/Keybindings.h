#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include "Keybinding.h"
#include <rct/List.h>
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
    List<Keybinding> mKeybindings;
    xkb_keysym_t mCurSym;
    uint16_t mCurMods;
};

#endif
