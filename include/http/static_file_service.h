#ifndef STATIC_FILE_SERVICE_H
#define STATIC_FILE_SERVICE_H

#include <string>

#include "http/http_request.h"
#include "http/http_response.h"

class StaticFileService
{
public:
    explicit StaticFileService(const std::string &doc_root = "");

    HttpResponse serve(const HttpRequest &request, const std::string &path) const;

private:
    std::string infer_content_type(const std::string &path) const;

private:
    std::string m_doc_root;
};

#endif
