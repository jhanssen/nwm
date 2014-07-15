#include "Workspace.h"
#include "GridLayout.h"
#include "StackLayout.h"
#include "WindowManager.h"
#include <rct/Log.h>
#include <assert.h>
#include <stdlib.h>

Workspace::Workspace(unsigned int layoutType, int screenNo, const Rect& rect, const String& name)
    : mRect(rect), mName(name), mLayout(0), mScreenNumber(screenNo)
{
    // error() << screenNo << rect;
    switch (layoutType) {
    case GridLayout::Type:
        mLayout = new GridLayout(mRect);
        break;
    case StackLayout::Type:
        mLayout = new StackLayout(mRect);
        break;
    default:
        error() << "Invalid layout type" << layoutType << "for Workspace";
        break;
    }
}

Workspace::~Workspace()
{
    delete mLayout;
}

void Workspace::setRect(const Rect& rect)
{
    mRect = rect;
    mLayout->setRect(rect);
}

void Workspace::updateFocus(Client *client)
{
    assert(client);
    if (client->noFocus())
        return;
    // find it in our list
    const auto it = mClients.find(client);
    assert(it != mClients.end());
    if (it == mClients.begin()) {
        // already focused, nothing to do
        return;
    }
    mClients.erase(it);
    mClients.prepend(client);
#warning should this tell WindowManager which client is the focused one?
}

void Workspace::deactivate()
{
    for (Client *client : mClients) {
        client->unmap();
    }
}

void Workspace::activate()
{
    WindowManager::instance()->activateWorkspace(this);
    // map all clients in the stacking order
    Client *client = 0;
    auto it = mClients.crbegin();
    const auto end = mClients.crend();
    while (it != end) {
        client = *it;
        client->map();
        ++it;
    }
    if (client)
        client->focus();
}

void Workspace::notifyRaised(Client *client)
{
    warning() << "raised" << client->className();
    auto it = mClients.find(client);
    assert(it != mClients.end());
    if (it != mClients.begin()) {
        mClients.erase(it);
        mClients.prepend(client);
    }
}

void Workspace::raise(RaiseMode mode)
{
    if (mClients.isEmpty())
        return;

    int pos = 0;
    switch (mode) {
    case Next:
        pos = 1;
        break;
    case Last:
        pos = -1;
        break;
    }
    if (!pos) {
        // nothing to do
        return;
    }
    int cur = 0;
    if (pos > 0) {
        auto it = mClients.begin();
        while (it != mClients.end()) {
            if (cur == pos) {
                (*it)->raise();
                (*it)->focus();
                return;
            }
            ++cur;
            ++it;
        }
    } else {
        pos = abs(pos) - 1;
        assert(pos >= 0);
        auto it = mClients.end();
        while (it != mClients.begin()) {
            --it;
            if (cur == pos) {
                (*it)->raise();
                (*it)->focus();
                return;
            }
            ++cur;
        }
    }
}

xcb_screen_t * Workspace::screen() const
{
    return WindowManager::instance()->screens().at(mScreenNumber);
}

inline bool Workspace::isActive() const
{
    return WindowManager::instance()->activeWorkspace(mScreenNumber) == this;
}

void Workspace::addClient(Client *client)
{
    assert(client);
    assert(client->screenNumber() == mScreenNumber);
    if (client->updateWorkspace(this)) {
        if (WindowManager::instance()->activeWorkspace(mScreenNumber) == this) {
            client->map();
        } else {
            client->unmap();
        }
        mClients.append(client);
    }
}
void Workspace::onClientDestroyed(Client *client)
{
    auto it = mClients.find(client);
    assert(it != mClients.end());
    const bool hadFocus = (it == mClients.begin());
    mClients.erase(it);
    if (hadFocus) {
    } else {
        // focus the first available one in our list
        for (Client *client : mClients) {
            if (!client->noFocus()) {
                client->focus();
                return;
            }
        }
        // No window to focus, focus the root window instead
        WindowManager *wm = WindowManager::instance();
        wm->updateCurrentScreen(mScreenNumber);
        const xcb_window_t root = screen()->root;
        xcb_set_input_focus(wm->connection(), XCB_INPUT_FOCUS_PARENT, root, wm->timestamp());
        // error() << "Setting input focus to root" << root;
        xcb_ewmh_set_active_window(wm->ewmhConnection(), mScreenNumber, root);
    }
}
