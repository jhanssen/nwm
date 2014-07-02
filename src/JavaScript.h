#ifndef JAVASCRIPT_H
#define JAVASCRIPT_H

#include <rct/rct-config.h>
#include <rct/ScriptEngine.h>
#include <rct/String.h>

class JavaScript : public ScriptEngine
{
public:
    JavaScript();
    ~JavaScript();

    Value evaluateFile(const Path &path, String *error);
};

#endif
