#include "Util.h"
#include <rct/Log.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

static inline char **c(const List<String> &args)
{
    char **ret = new char*[args.size() + 1];
    int i = 0;
    for (const auto &arg : args) {
        ret[i++] = strdup(arg.constData());
    }
    ret[i] = 0;
    return ret;
}

namespace Util {

void launch(const String& cmd, const Hash<String, String> &env)
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
            const char *shell = env.value("SHELL", 0).constData();
            if (!shell || !strlen(shell))
                shell = getenv("SHELL");
            if (!shell || !strlen(shell))
                shell = "/bin/sh";

            List<String> envList;
            for (const auto &it : env) {
                envList.append(it.first + '=' + it.second);
            }
            for (int i=0; environ[i]; ++i) {
                char *eq = strchr(environ[i], '=');
                String var;
                String value;
                if (!eq) {
                    var = environ[i];
                } else {
                    var.assign(environ[i], eq - environ[i]);
                    value = eq + 1;
                }
                if (!env.contains(var)) {
                    envList.append(var + '=' + value);
                }
            }
            execvpe(shell, c(List<String>() << shell << "-c" << cmd), c(envList));
        }
        exit(p == -1 ? 1 : 0);
        break; }
    default: {
        int status;
        waitpid(p, &status, 0);
        break; }
    }
}

} // namespace Util
