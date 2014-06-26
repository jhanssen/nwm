#include "Workspace.h"
#include <assert.h>

Workspace::WeakPtr Workspace::sActive;

Workspace::Workspace(const Size& size, const String& name)
    : mSize(size), mName(name)
{
    mLayout = std::make_shared<Layout>(mSize);
}

Workspace::~Workspace()
{
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
}

void Workspace::update()
{
}
