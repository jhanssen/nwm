#include "Commands.h"
#include "WindowManager.h"
#include "Keybinding.h"
#include <rct/EventLoop.h>
#include <rct/Log.h>
#include <rct/Path.h>
#include <confuse.h>

// static int validateKeybind(cfg_t *cfg, cfg_opt_t *opt)
// {
//     cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
//     error() << "validating" << cfg_title(sec);
//     return 0;
// }

static inline List<String> readStringList(cfg_t* cfg, const char* key)
{
    List<String> list;
    const int sz = cfg_size(cfg, key);
    for (int i = 0; i < sz; ++i) {
        list.append(cfg_getnstr(cfg, key, i));
    }
    return list;
}

int main(int argc, char** argv)
{
    if (!initLogging(argv[0], LogStderr, 0, 0, 0)) {
        fprintf(stderr, "Can't initialize logging\n");
        return 1;
    }

    // setup confuse parser
    cfg_opt_t keybindOpts[] = {
        // ewww. fixed recent versions of confuse I believe
        CFG_STR_LIST(const_cast<char*>("command"), const_cast<char*>("{}"), CFGF_NONE),
        CFG_STR(const_cast<char*>("exec"), 0, CFGF_NODEFAULT),
        CFG_END()
    };
    cfg_opt_t opts[] = {
        CFG_INT(const_cast<char*>("workspaces"), 1, CFGF_NONE),
        CFG_SEC(const_cast<char*>("keybind"), keybindOpts, CFGF_TITLE|CFGF_MULTI),
        CFG_END()
    };
    cfg_t* keybind;
    cfg_t* cfg = cfg_init(opts, CFGF_NONE);
    // cfg_set_validate_func(cfg, "keybind", validateKeybind);
    // Parse both /etc/nwm.conf and ~/.nwm.conf
    if (cfg_parse(cfg, "/etc/nwm.conf") == CFG_PARSE_ERROR) {
        error() << "Unable to parse /etc/nwm.conf";
        return 3;
    }
    if (cfg_parse(cfg, (Path::home() + "/.nwm.conf").constData()) == CFG_PARSE_ERROR) {
        error() << "Unable to parse" << (Path::home() + "/.nwm.conf");
        return 3;
    }

    WindowManager::SharedPtr manager;
    {
        EventLoop::SharedPtr loop = std::make_shared<EventLoop>();
        loop->init(EventLoop::MainEventLoop|EventLoop::EnableSigIntHandler);

        const int workspaces = cfg_getint(cfg, "workspaces");
        manager = std::make_shared<WindowManager>(workspaces);
        if (!manager->install(argc > 1 ? argv[1] : 0)) {
            error() << "Unable to install nwm. Another window manager already running?";
            return 2;
        }

        for (unsigned int i = 0; i < cfg_size(cfg, "keybind"); ++i) {
            keybind = cfg_getnsec(cfg, "keybind", i);
            const List<String> cmd = readStringList(keybind, "command");
            const char* exec = cfg_getstr(keybind, "exec");
            if (cmd.isEmpty() && !exec) {
                error() << "no command or exec for" << cfg_title(keybind);
                continue;
            }

            Keybinding keybinding(cfg_title(keybind), cmd, exec);
            if (!keybinding.isValid()) {
                error() << "keybind not valid" << cfg_title(keybind);
                continue;
            }
            manager->addKeybinding(keybinding);
        }

        Commands::initBuiltins();

        loop->exec();
    }

    cfg_free(cfg);

    if (manager) {
        WindowManager::release();
        manager.reset();
    }
    cleanupLogging();
    error() << "nwm done.";
    return 0;
}
