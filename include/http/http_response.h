#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>

struct HttpResponse
{
    int status_code;
    std::string status_text;
    std::string content_type;
    std::string body;
    std::string file_path;
    bool keep_alive;
    bool serve_file;

    HttpResponse()
        : status_code(500), keep_alive(false), serve_file(false)
    {
    }
};

#endif
