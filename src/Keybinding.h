#ifndef KEYBINDING
#define KEYBINDING

#include <rct/String.h>
#include <xcb/xcb.h>

class Keybinding
{
public:
    Keybinding(uint16_t mod, xcb_keysym_t sym, const String& cmd);
    Keybinding(const String& key, const String& cmd);

    bool isValid() const { return mValid; };

    uint16_t mod() const { return mMod; }
    xcb_keysym_t keysym() const { return mKeysym; }
    String command() const { return mCmd; }

private:
    bool parse(const String& key);

private:
    bool mValid;

    uint16_t mMod;
    xcb_keysym_t mKeysym;
    String mCmd;
};

#endif
