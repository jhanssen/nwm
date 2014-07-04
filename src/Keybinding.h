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
    };
    const List<Sequence>& sequence() const { return mSeq; }
    const Value& function() const { return mFunc; }

    void rebind(xcb_connection_t* conn, xcb_window_t win) const;

private:
    void parse(const String& key);
    void add(const String& key, const List<String>& mods = List<String>());

private:
    List<Sequence> mSeq;
    Value mFunc;
};

inline void Keybinding::rebind(xcb_connection_t* conn, xcb_window_t win) const
{
    for (const Keybinding::Sequence& seq : mSeq) {
        for (xcb_keycode_t code : seq.codes) {
            xcb_grab_key(conn, true, win, seq.mods, code, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }
    }
}

#endif
