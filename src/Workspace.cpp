#include "Workspace.h"
#include "GridLayout.h"
#include "StackLayout.h"
#include "WindowManager.h"
#include <rct/Log.h>
#include <assert.h>
#include <stdlib.h>

List<Workspace::WeakPtr> Workspace::sActive;

Workspace::Workspace(unsigned int layoutType, int screenNo, const Rect& rect, const String& name)
    : mRect(rect), mName(name), mScreenNumber(screenNo)
{
    if (sActive.isEmpty())
        sActive.resize(WindowManager::instance()->screenCount());
    error() << screenNo << rect;
    switch (layoutType) {
    case GridLayout::Type:
        mLayout = std::make_shared<GridLayout>(mRect);
        break;
    case StackLayout::Type:
        mLayout = std::make_shared<StackLayout>(mRect);
        break;
    default:
        error() << "Invalid layout type" << layoutType << "for Workspace";
        break;
    }
}

Workspace::~Workspace()
{
}

void Workspace::setRect(const Rect& rect)
{
    mRect = rect;
    mLayout->setRect(rect);
}

void Workspace::updateFocus(const Client::SharedPtr& client)
{
    if (client) {
        if (client->noFocus())
            return;
        // find it in our list
        bool first = true;
        auto it = mClients.begin();
        const auto end = mClients.cend();
        while (it != end) {
            if (Client::SharedPtr candidate = it->lock()) {
                if (candidate == client) {
                    // yay
                    if (first) {
                        // already focused, nothing to do
                        return;
                    }
                    break;
                }
                first = false;
            }
            ++it;
        }
        assert(it != end);
        mClients.erase(it);
        mClients.prepend(client);
        client->focus();
    } else {
        // focus the first available one in our list
        for (const Client::WeakPtr& candidate : mClients) {
            if (Client::SharedPtr client = candidate.lock()) {
                if (!client->noFocus()) {
                    client->focus();
                    return;
                }
            }
        }
        // No window to focus, focus the root window instead
        WindowManager *wm = WindowManager::instance();
        wm->updateCurrentScreen(mScreenNumber);
        const xcb_window_t root = screen()->root;
        xcb_set_input_focus(wm->connection(), XCB_INPUT_FOCUS_PARENT, root, wm->timestamp());
        xcb_ewmh_set_active_window(wm->ewmhConnection(), mScreenNumber, root);
    }
}

void Workspace::deactivate()
{
    // unmap all clients
    for (const Client::WeakPtr& client : mClients) {
        if (Client::SharedPtr c = client.lock()) {
            c->unmap();
        }
    }
}

void Workspace::activate()
{
    printf("[%s:%d]: void Workspace::activate()\n", __FILE__, __LINE__); fflush(stdout);
    if (Workspace::SharedPtr old = sActive[mScreenNumber].lock()) {
        assert(old != shared_from_this());
        old->deactivate();
    }
    sActive[mScreenNumber] = shared_from_this();
    // map all clients in the stacking order
    Client::SharedPtr client;
    auto it = mClients.crbegin();
    const auto end = mClients.crend();
    while (it != end) {
        if (client = it->lock()) {
            client->map();
        }
        ++it;
    }
    if (client)
        client->focus();
}

void Workspace::notifyRaised(const Client::SharedPtr& client)
{
    warning() << "raised" << client->className();
    // find and remove
    auto it = mClients.begin();
    while (it != mClients.end()) {
        if (Client::SharedPtr cand = it->lock()) {
            if (cand == client) {
                mClients.erase(it);
                break;
            }
            ++it;
        } else {
            it = mClients.erase(it);
        }
    }
    mClients.prepend(client);
}

void Workspace::raise(RaiseMode mode)
{
    if (mClients.empty())
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
            if (Client::SharedPtr client = it->lock()) {
                if (cur == pos) {
                    client->raise();
                    client->focus();
                    return;
                }
                ++cur;
                ++it;
            } else {
                // take it out, it's dead
                it = mClients.erase(it);
            }
        }
    } else {
        pos = abs(pos) - 1;
        assert(pos >= 0);
        auto it = mClients.end();
        for (;;) {
            assert(it != mClients.begin());
            --it;
            if (Client::SharedPtr client = it->lock()) {
                if (cur == pos) {
                    client->raise();
                    client->focus();
                    return;
                }
                ++cur;
            } else {
                // take it out, it's dead
                it = mClients.erase(it);
                if (mClients.isEmpty())
                    return;
            }
            if (it == mClients.begin())
                return;
        }
    }
}

xcb_screen_t * Workspace::screen() const
{
    return WindowManager::instance()->screens().at(mScreenNumber);
}
