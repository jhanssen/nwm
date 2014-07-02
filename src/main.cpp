#include "Commands.h"
#include "WindowManager.h"
#include "Keybinding.h"
#include <rct/EventLoop.h>
#include <rct/Log.h>
#include <rct/Path.h>
#include <rct/ScriptEngine.h>

// static int validateKeybind(cfg_t *cfg, cfg_opt_t *opt)
// {
//     cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
//     error() << "validating" << cfg_title(sec);
//     return 0;
// }

// static inline List<String> readStringList(cfg_t* cfg, const char* key)
// {
//     List<String> list;
//     const int sz = cfg_size(cfg, key);
//     for (int i = 0; i < sz; ++i) {
//         list.append(cfg_getnstr(cfg, key, i));
//     }
//     return list;
// }

int main(int argc, char** argv)
{
#if 0
    // setup confuse parser
    cfg_opt_t keybindOpts[] = {
        // ewww. fixed recent versions of confuse I believe
        CFG_STR_LIST(const_cast<char*>("command"), const_cast<char*>("{}"), CFGF_NONE),
        CFG_STR(const_cast<char*>("exec"), 0, CFGF_NODEFAULT),
        CFG_STR(const_cast<char*>("javascript"), 0, CFGF_NODEFAULT),
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
#endif

    WindowManager::SharedPtr manager = std::make_shared<WindowManager>();
    if (!manager->init(argc, argv))
        return 1;
    EventLoop::SharedPtr loop = std::make_shared<EventLoop>();
    loop->init(EventLoop::MainEventLoop|EventLoop::EnableSigIntHandler);
    loop->exec();

    if (manager) {
        WindowManager::release();
        manager.reset();
    }
    cleanupLogging();
    error() << "nwm done.";
    return 0;
}
