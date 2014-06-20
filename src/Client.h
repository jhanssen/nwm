#ifndef CLIENT_H
#define CLIENT_H

#include <rct/Hash.h>
#include <xcb/xcb.h>
#include <memory>

class Client : public std::enable_shared_from_this<Client>
{
public:
    typedef std::shared_ptr<Client> SharedPtr;
    typedef std::weak_ptr<Client> WeakPtr;

    ~Client();

    static SharedPtr client(xcb_window_t window);
    static SharedPtr manage(xcb_window_t window);
    static void release(xcb_window_t window);

    void release() { release(mWindow); mWindow = 0; }
    bool isValid() const { return mValid; }

private:
    Client(xcb_window_t win);

private:
    xcb_window_t mWindow;
    xcb_window_t mFrame;
    bool mValid;

    static Hash<xcb_window_t, Client::SharedPtr> sClients;
};

#endif
