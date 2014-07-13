#include "Keybindings.h"
#include "WindowManager.h"
#include <rct/List.h>
#include <rct/Log.h>

Keybindings::Keybindings()
    : mCurrentToggle(0), mGrabbed(false)
{
}

Keybindings::~Keybindings()
{
}

bool Keybindings::feed(xkb_keysym_t sym, uint16_t mods)
{
    WindowManager *wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    List<xcb_window_t> roots = wm->roots();

    mFeed.mSeq.append(Keybinding::Sequence({ mods, Set<xcb_keycode_t>(), sym }));
    // is the current sequence a prefix?
    if (!mGrabbed && mPrefixes.contains(mFeed.mSeq.back())) {
        // yes, grab the keyboard
        mGrabbed = true;
        for (xcb_window_t root : roots) {
            xcb_grab_keyboard_cookie_t cookie = xcb_grab_keyboard(conn, 1, root, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
            xcb_generic_error_t* err = 0;
            AutoPointer<xcb_grab_keyboard_reply_t> reply = xcb_grab_keyboard_reply(conn, cookie, &err);

            assert(!err);
        }

        xcb_allow_events(conn, XCB_ALLOW_ASYNC_KEYBOARD, XCB_CURRENT_TIME);
    }

    Set<Keybinding>& bindings = mCurrentToggle ? mToggles[*mCurrentToggle] : mKeybindings;

    const auto it = bindings.lower_bound(mFeed);
    if (it == bindings.end()) {
        // were done, ungrab if we're grabbed etc
        mFeed.mSeq.clear();
        if (mGrabbed) {
            mGrabbed = false;
            xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
        }
        return false;
    }
    // are we a candidate for the found binding?
    const Keybinding& found = *it;
    bool cand = false;

    if (mFeed.mSeq.size() <= found.mSeq.size()) {
        cand = true;
        auto cit = mFeed.mSeq.cbegin();
        const auto cend = mFeed.mSeq.cend();
        auto coit = found.mSeq.cbegin();
        const auto coend = found.mSeq.cend();
        while (cit != cend && coit != coend) {
            if (cit->mods != coit->mods) {
                cand = false;
                break;
            }
            if (cit->sym != coit->sym) {
                cand = false;
                break;
            }
            ++cit;
            ++coit;
        }
        assert(!cand
               || mFeed.mSeq.size() < found.mSeq.size()
               || (cit == cend && coit == coend));
    }
    if (cand) {
        if (mFeed.mSeq.size() == found.mSeq.size()) {
            // done, ungrab stuff
            if (mGrabbed) {
                mGrabbed = false;
                xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
            }
            // see if we're a toggle
            if (found.function().isInvalid()) {
                auto it = mToggles.find(found);
                if (it != mToggles.end()) {
                    // yes, we are
                    mCurrentToggle = &found;
                    // unbind everything and rebind our toggle keys + escape
                    rebindAll();
                    mFeed.mSeq.clear();
                    return false;
                }
            } else if (mCurrentToggle && found.function().isString() && found.function().toString() == "escape") {
                // bail out of the current toggle
                mCurrentToggle = 0;
                rebindAll();
                mFeed.mSeq.clear();
                return false;
            }
            return true;
        }
        // done, allow next event
        return false;
    }
    // not a candidate, ungrab stuff
    mFeed.mSeq.clear();
    if (mGrabbed) {
        mGrabbed = false;
        xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
    }
    return false;
}

const Keybinding* Keybindings::current()
{
    Set<Keybinding>& bindings = mCurrentToggle ? mToggles[*mCurrentToggle] : mKeybindings;

    const auto it = bindings.find(mFeed);
    if (it == bindings.end())
        return 0;
    mFeed.mSeq.clear();
    return &*it;
}

void Keybindings::add(const Keybinding& binding)
{
    WindowManager *wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();

    mKeybindings.insert(binding);
    if (mCurrentToggle)
        return;
    for (xcb_window_t root : wm->roots())
        rebind(binding, conn, root);
    const List<Client::SharedPtr>& clients = Client::clients();
    for (const Client::SharedPtr& client : clients) {
        rebind(binding, conn, client->window());
    }
}

void Keybindings::toggle(const Keybinding& binding, const Set<Keybinding>& subbindings)
{
    if (!mEscape.isValid()) {
        mEscape.init("Escape", "escape");
        assert(mEscape.isValid());
    }

    add(binding);
    mToggles[binding] = subbindings;
    mToggles[binding].insert(mEscape);
}

void Keybindings::rebindAll()
{
    mPrefixes.clear();

    WindowManager *wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    const List<Client::SharedPtr>& clients = Client::clients();
#warning clients should return a list per screen

    Set<Keybinding>& bindings = mCurrentToggle ? mToggles[*mCurrentToggle] : mKeybindings;

    for (xcb_window_t root : wm->roots()) {
        xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY);
        for (const Client::SharedPtr& client : clients) {
            if (client->root() == root)
                xcb_ungrab_key(conn, XCB_GRAB_ANY, client->window(), XCB_BUTTON_MASK_ANY);
        }

        for (const Keybinding& binding : bindings) {
            rebind(binding, conn, root);
            for (const Client::SharedPtr& client : clients) {
                if (client->root() == root) {
                    rebind(binding, conn, client->window());
                }
            }
        }
    }
}

void Keybindings::rebind(xcb_window_t win)
{
    Set<Keybinding>& bindings = mCurrentToggle ? mToggles[*mCurrentToggle] : mKeybindings;

    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_ungrab_key(conn, XCB_GRAB_ANY, win, XCB_BUTTON_MASK_ANY);
    for (const Keybinding& binding : bindings) {
        rebind(binding, conn, win);
    }
}

void Keybindings::rebind(const Keybinding& binding, xcb_connection_t* conn, xcb_window_t win)
{
    const auto& seqs = binding.sequence();
    if (seqs.isEmpty())
        return;
    const uint8_t keymode = (seqs.size() == 1) ? XCB_GRAB_MODE_ASYNC : XCB_GRAB_MODE_SYNC;
    const auto& seq = seqs.front();
    if (keymode == XCB_GRAB_MODE_SYNC) {
        if (mPrefixes.contains(seq))
            return;
        mPrefixes.insert(seq);
    }
    for (xcb_keycode_t code : seq.codes) {
        xcb_grab_key(conn, true, win, seq.mods, code, XCB_GRAB_MODE_ASYNC, keymode);
    }
}
