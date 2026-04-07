#include "http/user_repository.h"

#include <cstdio>

std::map<std::string, std::string> UserRepository::load_all(connection_pool *connPool)
{
    std::map<std::string, std::string> users;

    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        fprintf(stderr, "SELECT error:%s\n", mysql_error(mysql));
        return users;
    }

    MYSQL_RES *result = mysql_store_result(mysql);
    if (!result)
    {
        return users;
    }

    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        users[row[0]] = row[1];
    }
    mysql_free_result(result);
    return users;
}

bool UserRepository::insert(MYSQL *mysql, const std::string &username, const std::string &password)
{
    const std::string sql = "INSERT INTO user(username, passwd) VALUES('" + username + "', '" + password + "')";
    return mysql_query(mysql, sql.c_str()) == 0;
}
