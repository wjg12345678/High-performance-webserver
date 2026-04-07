#include "http/http_parser.h"

#include <cstdlib>
#include <cstring>

HttpParser::HttpParser()
{
    reset();
}

void HttpParser::reset()
{
    m_state = REQUEST_LINE;
    m_checked_idx = 0;
    m_content_length = 0;
    m_start_line = 0;
}

HttpParser::LineStatus HttpParser::parse_line(char *buffer, long read_idx)
{
    for (; m_checked_idx < read_idx; ++m_checked_idx)
    {
        const char temp = buffer[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == read_idx)
            {
                return LINE_OPEN;
            }
            if (buffer[m_checked_idx + 1] == '\n')
            {
                buffer[m_checked_idx++] = '\0';
                buffer[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        if (temp == '\n')
        {
            if (m_checked_idx > 0 && buffer[m_checked_idx - 1] == '\r')
            {
                buffer[m_checked_idx - 1] = '\0';
                buffer[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HttpParser::Result HttpParser::parse_request_line(char *text, HttpRequest &request)
{
    char *path = strpbrk(text, " \t");
    if (!path)
    {
        return BAD_REQUEST;
    }

    *path++ = '\0';
    if (strcasecmp(text, "GET") == 0)
    {
        request.method = HttpMethod::GET;
    }
    else if (strcasecmp(text, "POST") == 0)
    {
        request.method = HttpMethod::POST;
    }
    else
    {
        return BAD_REQUEST;
    }

    path += strspn(path, " \t");
    char *version = strpbrk(path, " \t");
    if (!version)
    {
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");
    if (strcasecmp(version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    if (strncasecmp(path, "http://", 7) == 0)
    {
        path += 7;
        path = strchr(path, '/');
    }
    else if (strncasecmp(path, "https://", 8) == 0)
    {
        path += 8;
        path = strchr(path, '/');
    }

    if (!path || path[0] != '/')
    {
        return BAD_REQUEST;
    }

    request.path = path;
    if (request.path == "/")
    {
        request.path = "/judge.html";
    }
    request.version = version;
    m_state = HEADERS;
    return NO_REQUEST;
}

HttpParser::Result HttpParser::parse_headers(char *text, HttpRequest &request)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_state = CONTENT;
            return NO_REQUEST;
        }
        return COMPLETE;
    }

    if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        request.keep_alive = (strcasecmp(text, "keep-alive") == 0);
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        request.host = text;
    }

    return NO_REQUEST;
}

HttpParser::Result HttpParser::parse_content(char *text, long read_idx, HttpRequest &request)
{
    if (read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        request.body = text;
        return COMPLETE;
    }
    return NO_REQUEST;
}

HttpParser::Result HttpParser::parse(char *buffer, long read_idx, HttpRequest &request)
{
    LineStatus line_status = LINE_OK;

    while ((m_state == CONTENT && line_status == LINE_OK) || ((line_status = parse_line(buffer, read_idx)) == LINE_OK))
    {
        char *text = get_line(buffer);
        m_start_line = static_cast<int>(m_checked_idx);

        switch (m_state)
        {
        case REQUEST_LINE:
        {
            Result result = parse_request_line(text, request);
            if (result == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case HEADERS:
        {
            Result result = parse_headers(text, request);
            if (result == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            if (result == COMPLETE)
            {
                return COMPLETE;
            }
            break;
        }
        case CONTENT:
        {
            Result result = parse_content(text, read_idx, request);
            if (result == COMPLETE)
            {
                return COMPLETE;
            }
            line_status = LINE_OPEN;
            break;
        }
        }
    }

    return line_status == LINE_BAD ? BAD_REQUEST : NO_REQUEST;
}
