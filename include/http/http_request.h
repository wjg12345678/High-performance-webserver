#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>

enum class HttpMethod
{
    GET,
    POST,
    UNKNOWN
};

struct HttpRequest
{
    HttpMethod method;
    std::string path;
    std::string version;
    std::string host;
    std::string body;
    bool keep_alive;

    HttpRequest()
    {
        reset();
    }

    void reset()
    {
        method = HttpMethod::UNKNOWN;
        path.clear();
        version.clear();
        host.clear();
        body.clear();
        keep_alive = false;
    }
};

#endif
