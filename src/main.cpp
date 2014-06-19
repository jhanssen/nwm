#include "WindowManager.h"
#include <rct/EventLoop.h>
#include <rct/Log.h>

int main(int argc, char** argv)
{
    if (!initLogging(argv[0], LogStderr, 0, 0, 0)) {
        fprintf(stderr, "Can't initialize logging\n");
        return 1;
    }

    EventLoop::SharedPtr loop = std::make_shared<EventLoop>();
    loop->init(EventLoop::MainEventLoop);

    WindowManager::SharedPtr manager = std::make_shared<WindowManager>();
    if (!manager->install()) {
        error() << "Unable to install nwm. Another window manager already running?";
        return 2;
    }

    loop->exec();
    cleanupLogging();
    return 0;
}
