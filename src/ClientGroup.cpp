#include "ClientGroup.h"
#include "Client.h"
#include "WindowManager.h"

Map<xcb_window_t, ClientGroup::WeakPtr> ClientGroup::sGroups;

void ClientGroup::raise()
{
    List<Client::SharedPtr> clients;
    for (const Client::WeakPtr& weak : mClients) {
        if (Client::SharedPtr shared = weak.lock()) {
            clients.append(shared);
        }
    }

    // sort by window type
    std::sort(clients.begin(), clients.end(),
              [](const Client::SharedPtr& a, const Client::SharedPtr& b) -> bool {
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
        xcb_configure_window(conn, (*it)->frame(), XCB_CONFIG_WINDOW_STACK_MODE, stackMode);
        ++it;
    }
}
