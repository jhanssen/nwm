#ifndef JAVASCRIPT_H
#define JAVASCRIPT_H

#include "Client.h"
#include <rct/rct-config.h>
#include <rct/ScriptEngine.h>
#include <rct/String.h>
#include <rct/Value.h>
#include <rct/Hash.h>

class JavaScript : public ScriptEngine
{
public:
    JavaScript();
    ~JavaScript();

    List<Path> configFiles() const { return mConfigFiles; }
    bool init(const List<Path> &configFiles, String *err = 0) { mConfigFiles = configFiles; return init(err); }
    bool reload(String *error = 0);
    Value evaluateFile(const Path &path, String *error);

    std::shared_ptr<Class> clientClass() const { return mClientClass; }
    std::shared_ptr<Class> fileClass() const { return mFileClass; }

    void onClient(Client *client, bool notify = true);
    void onClientEvent(Client *client, const String &event);
    void onClientRaised(Client *client) { onClientEvent(client, "raised"); }
    void onClientFocusLost(Client *client) { onClientEvent(client, "focusLost"); }
    void onClientFocused(Client *client) { onClientEvent(client, "focused"); }
    void onClientDestroyed(Client *client)
    {
        onClientEvent(client, "destroyed");
        mClients.remove(client);
    }
    void clear() { mClients.clear(); }
private:
    bool init(String *error);
    List<Path> mConfigFiles;
    std::shared_ptr<Class> mClientClass, mFileClass;
    Hash<String, Value> mOns;
    List<Client*> mClients;
};

#endif
