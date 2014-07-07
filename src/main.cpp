#include "WindowManager.h"
#include <rct/EventLoop.h>
#include <rct/Log.h>
#include <rct/Path.h>

int main(int argc, char** argv)
{
    while (true) {
        EventLoop::SharedPtr loop = std::make_shared<EventLoop>();
        loop->init(EventLoop::MainEventLoop|EventLoop::EnableSigIntHandler);

        WindowManager::SharedPtr manager = std::make_shared<WindowManager>();
        if (!manager->init(argc, argv))
            return 1;
        loop->exec();
        cleanupLogging();

        if (!manager->shouldRestart())
            break;
    }
    return 0;
}
