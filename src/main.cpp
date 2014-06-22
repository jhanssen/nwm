#include "WindowManager.h"
#include <rct/EventLoop.h>
#include <rct/Log.h>

int main(int argc, char** argv)
{
    if (!initLogging(argv[0], LogStderr, 0, 0, 0)) {
        fprintf(stderr, "Can't initialize logging\n");
        return 1;
    }

    WindowManager::SharedPtr manager;
    {
        EventLoop::SharedPtr loop = std::make_shared<EventLoop>();
        loop->init(EventLoop::MainEventLoop|EventLoop::EnableSigIntHandler);

        manager = std::make_shared<WindowManager>();
        if (!manager->install(argc > 1 ? argv[1] : 0)) {
            error() << "Unable to install nwm. Another window manager already running?";
            return 2;
        }

        loop->exec();
    }
    if (manager) {
        WindowManager::release();
        manager.reset();
    }
    cleanupLogging();
    error() << "nwm done.";
    return 0;
}
