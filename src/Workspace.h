#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "Client.h"
#include "Layout.h"
#include "Rect.h"
#include <rct/String.h>
#include <rct/LinkedList.h>
#include <rct/Set.h>
#include <memory>

class Workspace
{
public:
    Workspace(unsigned int layoutType, int screenNo, const Rect& rect, const String& name = String());
    ~Workspace();

    void setRect(const Rect& rect);

    void activate();

    int screenNumber() const { return mScreenNumber; }
    xcb_screen_t *screen() const;

    void addClient(Client *client);
    void removeClient(Client *client);

    void updateFocus(Client *client = 0);
    void onClientDestroyed(Client *client);

    enum RaiseMode { Next, Last };
    void raise(RaiseMode mode);
    void notifyRaised(Client *client);

    String name() const { return mName; }
    Rect rect() const { return mRect; }
    Layout *layout() const { return mLayout; }

    inline bool isActive() const;
private:
    void deactivate();

private:
    Rect mRect;
    String mName;
    Layout *mLayout;
    // ordered by focus
    LinkedList<Client *> mClients;
    const int mScreenNumber;
};

inline void Workspace::removeClient(Client *client)
{
    auto it = mClients.begin();
    const auto end = mClients.cend();
    while (it != end) {
        if (*it == client) {
            mClients.erase(it);
            return;
        }
        ++it;
    }
}

#endif
