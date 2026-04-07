#ifndef HTTP_ROUTER_H
#define HTTP_ROUTER_H

#include <string>

#include <mysql/mysql.h>

#include "db/sql_connection_pool.h"
#include "http/form_decoder.h"
#include "http/http_request.h"
#include "http/http_response.h"
#include "http/static_file_service.h"
#include "http/user_service.h"

class HttpRouter
{
public:
    explicit HttpRouter(const std::string &doc_root = "");

    HttpResponse route(const HttpRequest &request, MYSQL *mysql) const;
    static void preload_users(connection_pool *connPool);

private:
    HttpResponse handle_login(const HttpRequest &request) const;
    HttpResponse handle_register(const HttpRequest &request, MYSQL *mysql) const;

private:
    StaticFileService m_static_files;
    UserService m_users;
};

#endif
