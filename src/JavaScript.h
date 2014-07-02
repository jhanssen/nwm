#ifndef JAVASCRIPT_H
#define JAVASCRIPT_H

#include <rct/rct-config.h>
#include <rct/ScriptEngine.h>
#include <rct/String.h>

class JavaScript
{
public:
    JavaScript();
    ~JavaScript();

    void execute(const String& code);

private:
    void setup();

#ifdef HAVE_SCRIPTENGINE
    ScriptEngine engine;
#endif
};

#endif
