#include "Workspace.h"
#include <assert.h>

Workspace::WeakPtr Workspace::sActive;

Workspace::Workspace(const Rect& rect, const String& name)
    : mRect(rect), mName(name)
{
    mGridLayout = std::make_shared<GridLayout>(mRect);
}

Workspace::~Workspace()
{
}

void Workspace::setRect(const Rect& rect)
{
    mRect = rect;
    mGridLayout->setRect(rect);
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
    if (Workspace::SharedPtr old = sActive.lock()) {
        old->deactivate();
    }
    sActive = shared_from_this();
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
