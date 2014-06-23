#include "Keybinding.h"
#include "WindowManager.h"

Keybinding::Keybinding(uint16_t mod, xcb_keysym_t sym, const String& cmd)
    : mValid(true), mMod(mod), mKeysym(sym), mCmd(cmd)
{
}

Keybinding::Keybinding(const String& key, const String& cmd)
    : mMod(0), mKeysym(0), mCmd(cmd)
{
    mValid = parse(key);
}

bool Keybinding::parse(const String& key)
{
    return false;
}
