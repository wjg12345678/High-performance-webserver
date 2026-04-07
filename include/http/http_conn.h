#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <atomic>
#include <string>

#include "db/sql_connection_pool.h"
#include "http/http_parser.h"
#include "http/http_request.h"
#include "http/http_response.h"
#include "http/http_router.h"
#include "log/log.h"
#include "timer/lst_timer.h"

class http_conn
{
public:
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

public:
    http_conn();
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, std::string user, std::string passwd, std::string sqlname, int epollfd);
    void bind_client_data(client_data *client_data);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool);

private:
    void init();
    HttpParser::Result process_read();
    HttpResponse build_response();
    bool process_write(const HttpResponse &response);
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length, const char *content_type);
    bool add_content_type(const char *content_type);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_user_count;
    MYSQL *mysql;

private:
    int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int bytes_to_send;
    int bytes_have_send;
    std::string m_doc_root;
    std::string m_real_file;
    HttpRequest m_request;
    HttpParser m_parser;
    HttpRouter m_router;
    int m_TRIGMode;
    int m_close_log;
    client_data *m_client_data;
};

#endif
