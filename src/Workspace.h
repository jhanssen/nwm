#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "Client.h"
#include "Layout.h"
#include "Rect.h"
#include <rct/String.h>
#include <rct/List.h>
#include <rct/Set.h>
#include <memory>

class Workspace : public std::enable_shared_from_this<Workspace>
{
public:
    typedef std::shared_ptr<Workspace> SharedPtr;
    typedef std::weak_ptr<Workspace> WeakPtr;

    Workspace(const Size& size, const String& name = String());
    ~Workspace();

    void activate();

    void addClient(const Client::SharedPtr& client);
    void removeClient(const Client::SharedPtr& client);

    void updateFocus(const Client::SharedPtr& client = Client::SharedPtr());
    Client::SharedPtr focusedClient() const;

    String name() const { return mName; }
    Size size() const { return mSize; }
    Layout::SharedPtr layout() const { return mLayout; }

    static SharedPtr active() { return sActive.lock(); }

private:
    void deactivate();
    void update();

private:
    Size mSize;
    String mName;
    Layout::SharedPtr mLayout;
    // ordered by focus
    List<Client::WeakPtr> mClients;

    static Workspace::WeakPtr sActive;
};

inline Client::SharedPtr Workspace::focusedClient() const
{
    for (const Client::WeakPtr& client : mClients) {
        if (Client::SharedPtr c = client.lock())
            return c;
    }
    return Client::SharedPtr();
}

inline void Workspace::addClient(const Client::SharedPtr& client)
{
    mClients.append(client);
    if (sActive.lock() == shared_from_this())
        update();
}

inline void Workspace::removeClient(const Client::SharedPtr& client)
{
    auto it = mClients.begin();
    const auto end = mClients.cend();
    while (it != end) {
        if (it->lock() == client) {
            mClients.erase(it);
            if (sActive.lock() == shared_from_this())
                update();
            return;
        }
        ++it;
    }
}

inline void Workspace::activate()
{
    if (Workspace::SharedPtr old = sActive.lock()) {
        old->deactivate();
    }
    sActive = shared_from_this();
    update();
}

#endif
