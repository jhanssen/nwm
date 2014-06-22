#ifndef API_H
#define API_H

#include <memory>
#include <unordered_set>

class API
{
public:
    typedef std::shared_ptr<API> SharedPtr;
    typedef std::weak_ptr<API> WeakPtr;

    API();
    virtual ~API();

    static void registerAPI(const API::SharedPtr& api);
    static void unregisterAPI(const API::SharedPtr& api);
    static void clear() { sApis.clear(); }

private:
    static std::unordered_set<API::SharedPtr> sApis;
};

#endif
