#ifndef KEYBINDING
#define KEYBINDING

#include <rct/String.h>
#include <rct/List.h>
#include <rct/Set.h>
#include <rct/Value.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

typedef uint32_t xkb_keysym_t;
typedef uint8_t xcb_keycode_t;

class Keybindings;

class Keybinding
{
public:
    typedef std::shared_ptr<Keybinding> SharedPtr;
    typedef std::weak_ptr<Keybinding> WeakPtr;

    Keybinding() { };
    Keybinding(const String& key, const Value& func);

    bool isValid() const { return !mSeq.empty(); };
    void recreate();

    struct Sequence
    {
        uint16_t mods;
        Set<xcb_keycode_t> codes;

        xkb_keysym_t sym;
        void recreate(xcb_key_symbols_t* syms);

        bool operator<(const Sequence& other) const;
    };
    const List<Sequence>& sequence() const { return mSeq; }
    const Value& function() const { return mFunc; }

    bool operator<(const Keybinding& other) const;

    static uint16_t modToMask(const String& mod);

private:
    void parse(const String& key);
    void add(const String& key, const List<String>& mods = List<String>());

private:
    friend class Keybindings;

    List<Sequence> mSeq;
    Value mFunc;
};

inline bool Keybinding::Sequence::operator<(const Sequence& other) const
{
    if (mods < other.mods)
        return true;
    else if (mods > other.mods)
        return false;
    return sym < other.sym;
}

inline bool Keybinding::operator<(const Keybinding& other) const
{
    auto it = mSeq.cbegin();
    const auto end = mSeq.cend();
    auto oit = other.mSeq.cbegin();
    const auto oend = other.mSeq.cend();
    while (it != end && oit != oend) {
        if (*it < *oit)
            return true;
        ++it;
        ++oit;
    }
    return false;
}

#endif
