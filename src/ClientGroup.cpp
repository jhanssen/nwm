#include "ClientGroup.h"
#include "Client.h"
#include "WindowManager.h"
#include "Workspace.h"

Map<xcb_window_t, ClientGroup*> ClientGroup::sGroups;

void ClientGroup::restack(Client *client, xcb_stack_mode_t stackMode, Client *sibling)
{
    warning() << "got restack" << client << stackMode << sibling;
    assert(client);
    if (sibling && (sibling == client || sibling->group() == this))
        sibling = 0;

    const bool clientIsDialog = client->isDialog();

    List<Client *> clients;
    for (Client *c : mClients) {
        if (c != client) {
            if (sibling == c)
                sibling = 0;
#warning this might not be right
            clients.append(c);
        }
    }

    // sort by window type
    std::sort(clients.begin(), clients.end(),
              [](Client *a, Client *b) -> bool {
                  if (a->isDialog() != b->isDialog())
                      return b->isDialog();
                  return a->window() < b->window();
              });


    // // raise in order
    xcb_connection_t* conn = WindowManager::instance()->connection();
    uint32_t values[2];
    uint8_t valueMask = XCB_CONFIG_WINDOW_STACK_MODE;
    if (sibling) {
        values[0] = sibling->frame();
        values[1] = stackMode;
        valueMask |= XCB_CONFIG_WINDOW_SIBLING;
    } else {
        values[0] = stackMode;
    }
    auto it = clients.cbegin();
    const auto end = clients.cend();
    while (it != end) {
        // break at the first client that's supposed to be on top of our client
        if (!clientIsDialog && (*it)->isDialog())
            break;
        warning() << "restack regular" << *it;
        xcb_configure_window(conn, (*it)->frame(), valueMask, values);
        ++it;
    }

    // raise the selected client
    warning() << "restacking selected client" << client << client->className();
    xcb_configure_window(conn, client->frame(), valueMask, values);
    if (Workspace *ws = client->workspace())
        ws->notifyRaised(client);
    WindowManager::instance()->js().onClientRaised(client);

    // raise any remaining dialogs
    while (it != end) {
        assert((*it)->isDialog());
        warning() << "raising dialog" << *it;
        xcb_configure_window(conn, (*it)->frame(), valueMask, values);
        ++it;
    }
}
