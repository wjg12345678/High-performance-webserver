#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "http/http_request.h"

class HttpParser
{
public:
    enum Result
    {
        NO_REQUEST,
        COMPLETE,
        BAD_REQUEST
    };

    HttpParser();

    void reset();
    Result parse(char *buffer, long read_idx, HttpRequest &request);

private:
    enum ParseState
    {
        REQUEST_LINE,
        HEADERS,
        CONTENT
    };

    enum LineStatus
    {
        LINE_OK,
        LINE_BAD,
        LINE_OPEN
    };

    LineStatus parse_line(char *buffer, long read_idx);
    Result parse_request_line(char *text, HttpRequest &request);
    Result parse_headers(char *text, HttpRequest &request);
    Result parse_content(char *text, long read_idx, HttpRequest &request);
    char *get_line(char *buffer) { return buffer + m_start_line; }

private:
    ParseState m_state;
    long m_checked_idx;
    long m_content_length;
    int m_start_line;
};

#endif
