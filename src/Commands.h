#ifndef COMMANDS_H
#define COMMANDS_H

#include <rct/Hash.h>
#include <rct/Log.h>
#include <rct/String.h>
#include <functional>

class Commands
{
public:
    static void add(const String& name, std::function<void()>&& func);
    static void exec(const String& name);

    static void initBuiltins();

private:
    static Hash<String, std::function<void()> > sCmds;
};

inline void Commands::add(const String& name, std::function<void()>&& func)
{
    sCmds[name] = std::move(func);
}

inline void Commands::exec(const String& name)
{
    const auto it = sCmds.find(name);
    if (it == sCmds.end()) {
        error() << "No such command:" << name;
        return;
    }
    it->second();
}

#endif
