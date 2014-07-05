#ifndef CLIENTGROUP_H
#define CLIENTGROUP_H

#include <rct/List.h>
#include <rct/Map.h>
#include <memory>
#include <xcb/xcb.h>

class Client;

class ClientGroup : public std::enable_shared_from_this<ClientGroup>
{
public:
    typedef std::shared_ptr<ClientGroup> SharedPtr;
    typedef std::weak_ptr<ClientGroup> WeakPtr;

    ~ClientGroup() { }

    static SharedPtr clientGroup(xcb_window_t leader);
    void add(const std::shared_ptr<Client>& client) { mClients.append(client); }
    const List<std::weak_ptr<Client> >& clients() const { return mClients; }

private:
    ClientGroup() { }

private:
    List<std::weak_ptr<Client> > mClients;

    static Map<xcb_window_t, WeakPtr> sGroups;
};

inline ClientGroup::SharedPtr ClientGroup::clientGroup(xcb_window_t leader)
{
    auto it = sGroups.find(leader);
    if (it != sGroups.end()) {
        if (SharedPtr ptr = it->second.lock()) {
            return ptr;
        }
        sGroups.erase(it);
    }
    SharedPtr ptr(new ClientGroup);
    sGroups[leader] = ptr;
    return ptr;
}

#endif
