#include "Keybinding.h"
#include "WindowManager.h"
#include <rct/Log.h>
#include <xkbcommon/xkbcommon.h>

Keybinding::Keybinding(const String& key, const String& cmd)
    : mCmd(cmd)
{
    parse(key);
}

void Keybinding::parse(const String& key)
{
    // parse expressions in the form of
    // Mod-Key Mod-Key

    const List<String>& seqs = key.split(' ');
    for (const String& seq : seqs) {
        List<String> keys = seq.split('-');
        switch (keys.size()) {
        case 0:
            error() << "Unable to parse key sequence" << seq << "from" << key;
            break;
        case 1:
            add(keys.at(0));
            break;
        default: {
            const String key = keys.takeLast();
            add(key, keys);
            break; }
        }
    }
}

static inline uint16_t modToMask(const String& mod)
{
    if (mod == "Shift")
        return XCB_MOD_MASK_SHIFT;
    else if (mod == "Ctrl" || mod == "Control")
        return XCB_MOD_MASK_CONTROL;
    else if (mod == "Mod1" || mod == "Alt")
        return XCB_MOD_MASK_1;
    else if (mod == "Mod2")
        return XCB_MOD_MASK_2;
    else if (mod == "Mod3")
        return XCB_MOD_MASK_3;
    else if (mod == "Mod4")
        return XCB_MOD_MASK_4;
    else if (mod == "Mod5")
        return XCB_MOD_MASK_5;
    else if (mod == "Lock")
        return XCB_MOD_MASK_LOCK;
    else if (mod == "Any")
        return XCB_MOD_MASK_ANY;
    error() << "Couldn't parse modifier" << mod;
    return XCB_NO_SYMBOL;
}

void Keybinding::add(const String& key, const List<String>& mods)
{
    const xkb_keysym_t sym = xkb_keysym_from_name(key.constData(), XKB_KEYSYM_CASE_INSENSITIVE);
    if (sym == XKB_KEY_NoSymbol) {
        error() << "Unable to get keysym for" << key;
        return;
    }
    mSeq.resize(mSeq.size() + 1);
    Sequence& seq = mSeq.last();
    seq.mods = 0;
    for (const String& mod : mods) {
        seq.mods |= modToMask(mod);
    }
    seq.sym = sym;
    xcb_key_symbols_t* syms = WindowManager::instance()->keySymbols();
    seq.recreate(syms);
}

void Keybinding::Sequence::recreate(xcb_key_symbols_t* syms)
{
    codes.clear();

    xcb_keycode_t* keycodes = xcb_key_symbols_get_keycode(syms, sym);
    if (!keycodes) {
        error() << "Couldn't get keycode for" << sym;
        return;
    }
    for (xcb_keycode_t* code = keycodes; *code; ++code) {
        codes.append(*code);
    }
    free(keycodes);
}

void Keybinding::recreate()
{
    xcb_key_symbols_t* syms = WindowManager::instance()->keySymbols();
    for (Sequence& seq : mSeq) {
        seq.recreate(syms);
    }
}
