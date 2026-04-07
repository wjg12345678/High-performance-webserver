#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include <string>

#include <mysql/mysql.h>

#include "db/sql_connection_pool.h"
#include "http/user_repository.h"

class UserService
{
public:
    static void preload(connection_pool *connPool);
    bool login(const std::string &username, const std::string &password) const;
    bool register_user(MYSQL *mysql, const std::string &username, const std::string &password) const;
};

#endif
