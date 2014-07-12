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

    const Class::SharedPtr& clientClass() const { return mClientClass; }
    const Class::SharedPtr& fileClass() const { return mFileClass; }

    void onClient(const Client::SharedPtr& client);
    void onClientEvent(const Client::SharedPtr &client, const String &event);
    void onClientRaised(const Client::SharedPtr &client) { onClientEvent(client, "raised"); }
    void onClientFocusLost(const Client::SharedPtr &client) { onClientEvent(client, "focusLost"); }
    void onClientFocused(const Client::SharedPtr &client) { onClientEvent(client, "focused"); }
    void onClientDestroyed(const Client::SharedPtr &client)
    {
        onClientEvent(client, "destroyed");
        mClients.remove(client);
    }
    void clear() { mClients.clear(); }
private:
    bool init(String *error);
    List<Path> mConfigFiles;
    Class::SharedPtr mClientClass, mFileClass;
    Hash<String, Value> mOns;
    List<Client::SharedPtr> mClients;
};

#endif
