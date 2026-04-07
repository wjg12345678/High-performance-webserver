#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

#include <map>
#include <string>

#include <mysql/mysql.h>

#include "db/sql_connection_pool.h"

class UserRepository
{
public:
    static std::map<std::string, std::string> load_all(connection_pool *connPool);
    static bool insert(MYSQL *mysql, const std::string &username, const std::string &password);
};

#endif
