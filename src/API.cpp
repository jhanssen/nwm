#include "API.h"
#include <assert.h>

std::unordered_set<API::SharedPtr> API::sApis;

API::API()
{
}

API::~API()
{
}

void API::registerAPI(const API::SharedPtr& api)
{
    assert(!sApis.count(api));
    sApis.insert(api);
}

void API::unregisterAPI(const API::SharedPtr& api)
{
    sApis.erase(api);
}
