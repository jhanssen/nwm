#include "WindowManager.h"
#include <rct/EventLoop.h>
#include <rct/Log.h>
#include <rct/Path.h>

int main(int argc, char** argv)
{
    while (true) {
        EventLoop::SharedPtr loop = std::make_shared<EventLoop>();
        loop->init(EventLoop::MainEventLoop|EventLoop::EnableSigIntHandler);

        WindowManager manager;
        if (!manager.init(argc, argv)) {
            printf("[%s:%d]: if (!manager.init(argc, argv))\n", __FILE__, __LINE__); fflush(stdout);
            return 1;
        }
        loop->exec();
        printf("[%s:%d]: loop->exec();\n", __FILE__, __LINE__); fflush(stdout);
        cleanupLogging();

        if (!manager.shouldRestart()) {
            printf("[%s:%d]: if (!manager.shouldRestart()) {\n", __FILE__, __LINE__); fflush(stdout);
            break;
        } else {
            printf("[%s:%d]: } else {\n", __FILE__, __LINE__); fflush(stdout);
        }
    }
    return 0;
}
