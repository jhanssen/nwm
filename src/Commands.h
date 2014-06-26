#ifndef COMMANDS_H
#define COMMANDS_H

#include <rct/Hash.h>
#include <rct/List.h>
#include <rct/Log.h>
#include <rct/String.h>
#include <functional>

class Commands
{
public:
    static void add(const String& name, std::function<void(const List<String>&)>&& func);
    static void exec(const String& name, const List<String>& args = List<String>());

    static void initBuiltins();

private:
    static Hash<String, std::function<void(const List<String>&)> > sCmds;
};

inline void Commands::add(const String& name, std::function<void(const List<String>&)>&& func)
{
    sCmds[name] = std::move(func);
}

inline void Commands::exec(const String& name, const List<String>& args)
{
    const auto it = sCmds.find(name);
    if (it == sCmds.end()) {
        error() << "No such command:" << name;
        return;
    }
    it->second(args);
}

#endif
