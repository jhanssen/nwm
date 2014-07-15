#ifndef CLIENTGROUP_H
#define CLIENTGROUP_H

#include <rct/List.h>
#include <rct/Map.h>
#include <memory>
#include <xcb/xcb.h>
#include <rct/EventLoop.h>

class Client;

class ClientGroup
{
public:
    inline ~ClientGroup();

    static ClientGroup *clientGroup(xcb_window_t leader);
    void add(Client *client) { mClients.append(client); }
    const List<Client*> clients() const { return mClients; }

    xcb_window_t leader() const { return mLeader; }

    void raise(Client *client);
    void onClientDestroyed(Client *client);
private:
    ClientGroup(xcb_window_t leader) : mLeader(leader) { }

private:
    xcb_window_t mLeader;
    List<Client *> mClients;

    static Map<xcb_window_t, ClientGroup*> sGroups;
};

inline ClientGroup *ClientGroup::clientGroup(xcb_window_t leader)
{
    ClientGroup *&group = sGroups[leader];
    if (!group)
        group = new ClientGroup(leader);
    return group;
}

inline ClientGroup::~ClientGroup()
{
    if (mLeader)
        sGroups.remove(mLeader);
}

inline void ClientGroup::onClientDestroyed(Client *client)
{
    mClients.remove(client);
    if (mClients.isEmpty()) {
        sGroups.remove(mLeader);
        mLeader = 0;
        EventLoop::deleteLater(this);
    }
}



#endif
