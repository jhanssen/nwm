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

    void init();
    Value evaluateFile(const Path &path, String *error);

    void onClient(const Client::SharedPtr& client);
    const Class::SharedPtr& clientClass() const { return mClientClass; }

private:
    Class::SharedPtr mClientClass;
    Hash<String, Value> mOns;
};

#endif
