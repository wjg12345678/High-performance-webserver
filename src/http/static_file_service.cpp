#include "http/static_file_service.h"

StaticFileService::StaticFileService(const std::string &doc_root)
    : m_doc_root(doc_root)
{
}

HttpResponse StaticFileService::serve(const HttpRequest &request, const std::string &path) const
{
    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.content_type = infer_content_type(path);
    response.file_path = m_doc_root + path;
    response.keep_alive = request.keep_alive;
    response.serve_file = true;
    return response;
}

std::string StaticFileService::infer_content_type(const std::string &path) const
{
    const size_t dot = path.find_last_of('.');
    const std::string ext = dot == std::string::npos ? "" : path.substr(dot + 1);
    if (ext == "html")
        return "text/html";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "gif")
        return "image/gif";
    if (ext == "png")
        return "image/png";
    if (ext == "ico")
        return "image/x-icon";
    if (ext == "mp4")
        return "video/mp4";
    return "text/plain";
}
