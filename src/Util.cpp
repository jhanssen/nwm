#include "Util.h"
#include <unistd.h>
#include <stdlib.h>

namespace Util {

void launch(const String& cmd, const String& dpy)
{
    pid_t p = fork();
    switch (p) {
    case -1:
        return;
    case 0: {
        if (setsid() == -1) {
            // bad
            exit(1);
        }
        p = fork();
        if (p == 0) {
            const char* shell = getenv("SHELL");
            if (!shell)
                shell = "/bin/sh";
            if (!dpy.isEmpty()) {
                setenv("DISPLAY", dpy.constData(), 1);
            }
            execl(shell, "sh", "-c", cmd.constData());
        }
        exit(p == -1 ? 1 : 0);
        break; }
    default:
        break;
    }
}

} // namespace Util
