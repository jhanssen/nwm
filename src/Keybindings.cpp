#include "Keybindings.h"
#include "WindowManager.h"

Keybindings::Keybindings()
{
}

Keybindings::~Keybindings()
{
}

bool Keybindings::feed(xkb_keysym_t sym, uint16_t mods)
{
    mCurSym = sym;
    mCurMods = mods;
    return false;
}

const Keybinding* Keybindings::current()
{
    for (const Keybinding& binding : mKeybindings) {
        const List<Keybinding::Sequence>& seqs = binding.sequence();
        for (const Keybinding::Sequence& seq : seqs) {
            if (seq.sym == mCurSym && seq.mods == mCurMods)
                return &binding;
        }
    }
    return 0;
}

void Keybindings::add(const Keybinding& binding)
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();

    mKeybindings.append(binding);
    binding.rebind(conn, wm->screen()->root);
    const List<Client::SharedPtr>& clients = Client::clients();
    for (const Client::SharedPtr& client : clients) {
        binding.rebind(conn, client->window());
    }
}

void Keybindings::rebindAll()
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    xcb_connection_t* conn = wm->connection();
    xcb_window_t root = wm->screen()->root;

    const List<Client::SharedPtr>& clients = Client::clients();

    xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY);
    for (const Client::SharedPtr& client : clients) {
        xcb_ungrab_key(conn, XCB_GRAB_ANY, client->window(), XCB_BUTTON_MASK_ANY);
    }

    for (const Keybinding& binding : mKeybindings) {
        binding.rebind(conn, root);
        for (const Client::SharedPtr& client : clients) {
            binding.rebind(conn, client->window());
        }
    }
}

void Keybindings::rebind(xcb_window_t win)
{
    xcb_connection_t* conn = WindowManager::instance()->connection();
    xcb_ungrab_key(conn, XCB_GRAB_ANY, win, XCB_BUTTON_MASK_ANY);
    for (const Keybinding& binding : mKeybindings) {
        binding.rebind(conn, win);
    }
}
