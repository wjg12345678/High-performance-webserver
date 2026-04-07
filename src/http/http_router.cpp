#include "http/http_router.h"

#include <map>

HttpRouter::HttpRouter(const std::string &doc_root)
    : m_static_files(doc_root)
{
}

void HttpRouter::preload_users(connection_pool *connPool)
{
    UserService::preload(connPool);
}

HttpResponse HttpRouter::handle_login(const HttpRequest &request) const
{
    const std::map<std::string, std::string> form = FormDecoder::parse_urlencoded(request.body);
    const std::string username = form.count("user") ? form.find("user")->second : "";
    const std::string password = form.count("password") ? form.find("password")->second : "";
    return m_static_files.serve(request, m_users.login(username, password) ? "/welcome.html" : "/logError.html");
}

HttpResponse HttpRouter::handle_register(const HttpRequest &request, MYSQL *mysql) const
{
    const std::map<std::string, std::string> form = FormDecoder::parse_urlencoded(request.body);
    const std::string username = form.count("user") ? form.find("user")->second : "";
    const std::string password = form.count("password") ? form.find("password")->second : "";

    if (!mysql)
    {
        HttpResponse response;
        response.status_code = 500;
        response.status_text = "Internal Error";
        response.content_type = "text/plain";
        response.body = "database connection unavailable\n";
        response.keep_alive = request.keep_alive;
        return response;
    }

    return m_static_files.serve(
        request,
        m_users.register_user(mysql, username, password) ? "/log.html" : "/registerError.html");
}

HttpResponse HttpRouter::route(const HttpRequest &request, MYSQL *mysql) const
{
    if (request.path == "/0")
        return m_static_files.serve(request, "/register.html");
    if (request.path == "/1")
        return m_static_files.serve(request, "/log.html");
    if (request.path == "/5")
        return m_static_files.serve(request, "/picture.html");
    if (request.path == "/6")
        return m_static_files.serve(request, "/video.html");
    if (request.path == "/7")
        return m_static_files.serve(request, "/fans.html");

    if (request.method == HttpMethod::POST && request.path == "/2CGISQL.cgi")
    {
        return handle_login(request);
    }

    if (request.method == HttpMethod::POST && request.path == "/3CGISQL.cgi")
    {
        return handle_register(request, mysql);
    }

    return m_static_files.serve(request, request.path);
}
