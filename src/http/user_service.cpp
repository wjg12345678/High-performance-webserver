#include "http/user_service.h"

#include <map>

#include "common/locker.h"

namespace
{
locker g_user_lock;
std::map<std::string, std::string> g_users;
}

void UserService::preload(connection_pool *connPool)
{
    g_user_lock.lock();
    g_users = UserRepository::load_all(connPool);
    g_user_lock.unlock();
}

bool UserService::login(const std::string &username, const std::string &password) const
{
    g_user_lock.lock();
    const std::map<std::string, std::string>::const_iterator it = g_users.find(username);
    const bool ok = it != g_users.end() && it->second == password;
    g_user_lock.unlock();
    return ok;
}

bool UserService::register_user(MYSQL *mysql, const std::string &username, const std::string &password) const
{
    g_user_lock.lock();
    if (g_users.find(username) != g_users.end())
    {
        g_user_lock.unlock();
        return false;
    }

    const bool inserted = UserRepository::insert(mysql, username, password);
    if (inserted)
    {
        g_users[username] = password;
    }
    g_user_lock.unlock();
    return inserted;
}
