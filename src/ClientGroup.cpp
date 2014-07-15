#include "ClientGroup.h"
#include "Client.h"
#include "WindowManager.h"
#include "Workspace.h"

Map<xcb_window_t, ClientGroup*> ClientGroup::sGroups;

void ClientGroup::raise(Client *client)
{
    const bool clientIsDialog = client->isDialog();
    const bool clientIsFloating = client->isFloating();

    List<Client *> clients;
    for (Client *c : mClients) {
        if (c != client)
            clients.append(c);
    }

    // sort by window type
    std::sort(clients.begin(), clients.end(),
              [](Client *a, Client *b) -> bool {
                  if (a->isDialog() != b->isDialog())
                      return b->isDialog();
                  if (a->isFloating() != b->isFloating())
                      return b->isFloating();
                  return a->window() < b->window();
              });

    // raise in order
    xcb_connection_t* conn = WindowManager::instance()->connection();
    const uint32_t stackMode[] = { XCB_STACK_MODE_ABOVE };
    auto it = clients.cbegin();
    const auto end = clients.cend();
    while (it != end) {
        // break at the first client that's supposed to be on top of our client
        if (!clientIsDialog && (*it)->isDialog())
            break;
        else if (!clientIsFloating && (*it)->isFloating())
            break;
        warning() << "raising regular" << *it << "floating" << (*it)->isFloating();
        xcb_configure_window(conn, (*it)->frame(), XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
        // if (Workspace * ws = (*it)->workspace())
        //     ws->notifyRaised(*it);
        ++it;
    }

    // raise the selected client
    warning() << "raising selected client" << client << client->className();
    xcb_configure_window(conn, client->frame(), XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
    if (Workspace *ws = client->workspace())
        ws->notifyRaised(client);
    WindowManager::instance()->js().onClientRaised(client);

    // raise any remaining dialogs
    while (it != end) {
        assert((*it)->isDialog() || (*it)->isFloating());
        warning() << "raising dialog/floating" << *it;
        xcb_configure_window(conn, (*it)->frame(), XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
        // if (Workspace *ws = (*it)->workspace())
        //     ws->notifyRaised(*it);
        ++it;
    }
}
