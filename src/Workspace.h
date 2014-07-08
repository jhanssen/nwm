#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "Client.h"
#include "Layout.h"
#include "Rect.h"
#include <rct/String.h>
#include <rct/LinkedList.h>
#include <rct/Set.h>
#include <memory>

class Workspace : public std::enable_shared_from_this<Workspace>
{
public:
    typedef std::shared_ptr<Workspace> SharedPtr;
    typedef std::weak_ptr<Workspace> WeakPtr;

    Workspace(unsigned int layoutType, int screenNo, const Rect& rect, const String& name = String());
    ~Workspace();

    void setRect(const Rect& rect);

    void activate();

    int screenNumber() const { return mScreenNumber; }
    xcb_screen_t *screen() const;

    void addClient(const Client::SharedPtr& client);
    void removeClient(const Client::SharedPtr& client);

    void updateFocus(const Client::SharedPtr& client = Client::SharedPtr());

    enum RaiseMode { Next, Last };
    void raise(RaiseMode mode);
    void notifyRaised(const Client::SharedPtr& client);

    String name() const { return mName; }
    Rect rect() const { return mRect; }
    Layout::SharedPtr layout() const { return mLayout; }

    static SharedPtr active(int screenNumber) { return sActive.at(screenNumber).lock(); }

    inline bool isActive() const { return sActive.at(mScreenNumber).lock() == shared_from_this(); }

private:
    void deactivate();

private:
    Rect mRect;
    String mName;
    Layout::SharedPtr mLayout;
    // ordered by focus
    LinkedList<Client::WeakPtr> mClients;
    const int mScreenNumber;

    static List<Workspace::WeakPtr> sActive;
};

inline void Workspace::addClient(const Client::SharedPtr& client)
{
    assert(client);
    assert(client->screenNumber() == mScreenNumber);
    Workspace::SharedPtr that = shared_from_this();
    if (client->updateWorkspace(that)) {
        if (active(mScreenNumber) == shared_from_this()) {
            client->map();
        } else {
            client->unmap();
        }
        mClients.append(client);
    }
}

inline void Workspace::removeClient(const Client::SharedPtr& client)
{
    auto it = mClients.begin();
    const auto end = mClients.cend();
    while (it != end) {
        if (it->lock() == client) {
            mClients.erase(it);
            return;
        }
        ++it;
    }
}

#endif
