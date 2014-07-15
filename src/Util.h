#ifndef UTIL_H
#define UTIL_H

#include <rct/String.h>
#include <rct/Hash.h>

namespace Util {
void launch(const String &cmd, const Hash<String, String> &env);
}

#endif
