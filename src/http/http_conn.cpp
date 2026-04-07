#include "http/http_conn.h"

#include <chrono>
#include <cstring>

#include "server/server_stats.h"

namespace
{
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int trig_mode)
{
    epoll_event event;
    event.data.fd = fd;

    if (trig_mode == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev, int trig_mode)
{
    epoll_event event;
    event.data.fd = fd;

    if (trig_mode == 1)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

const char *default_error_title(int status_code)
{
    switch (status_code)
    {
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    default:
        return "Internal Error";
    }
}

const char *default_error_body(int status_code)
{
    switch (status_code)
    {
    case 400:
        return "Your request has bad syntax or is inherently impossible to staisfy.\n";
    case 403:
        return "You do not have permission to get file form this server.\n";
    case 404:
        return "The requested file was not found on this server.\n";
    default:
        return "There was an unusual problem serving the request file.\n";
    }
}

HttpResponse make_error_response(int status_code, bool keep_alive)
{
    HttpResponse response;
    response.status_code = status_code;
    response.status_text = default_error_title(status_code);
    response.content_type = "text/plain";
    response.body = default_error_body(status_code);
    response.keep_alive = keep_alive;
    return response;
}
}

int http_conn::m_user_count = 0;

http_conn::http_conn()
    : mysql(NULL),
      m_epollfd(-1),
      m_sockfd(-1),
      m_read_idx(0),
      m_write_idx(0),
      m_linger(false),
      m_file_address(NULL),
      m_iv_count(0),
      bytes_to_send(0),
      bytes_have_send(0),
      m_TRIGMode(0),
      m_close_log(0),
      m_client_data(NULL)
{
    memset(&m_address, 0, sizeof(m_address));
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(&m_file_stat, 0, sizeof(m_file_stat));
}

void http_conn::initmysql_result(connection_pool *connPool)
{
    HttpRouter::preload_users(connPool);
}

void http_conn::bind_client_data(client_data *client_data)
{
    m_client_data = client_data;
}

void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
        ServerStats::get_instance().conn_closed();
    }

    if (m_client_data)
    {
        ++m_client_data->timer_version;
        m_client_data->conn = NULL;
        m_client_data->timer = NULL;
        m_client_data->sockfd = -1;
        m_client_data = NULL;
    }
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int trig_mode,
                     int close_log, std::string user, std::string passwd, std::string sqlname, int epollfd)
{
    (void)user;
    (void)passwd;
    (void)sqlname;

    m_sockfd = sockfd;
    m_address = addr;
    m_epollfd = epollfd;
    m_TRIGMode = trig_mode;
    m_close_log = close_log;
    m_doc_root = root;
    m_router = HttpRouter(m_doc_root);

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;
    ServerStats::get_instance().conn_opened();

    init();
}

void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_linger = false;
    m_read_idx = 0;
    m_write_idx = 0;
    m_iv_count = 0;
    m_real_file.clear();
    m_request.reset();
    m_parser.reset();

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(&m_file_stat, '\0', sizeof(m_file_stat));
    m_file_address = NULL;
}

bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    if (m_TRIGMode == 0)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read <= 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
        return true;
    }

    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

HttpParser::Result http_conn::process_read()
{
    return m_parser.parse(m_read_buf, m_read_idx, m_request);
}

HttpResponse http_conn::build_response()
{
    HttpResponse response = m_router.route(m_request, mysql);
    m_linger = response.keep_alive;
    return response;
}

bool http_conn::process_write(const HttpResponse &response)
{
    m_write_idx = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_iv_count = 0;

    if (!add_status_line(response.status_code, response.status_text.c_str()))
    {
        return false;
    }

    if (response.serve_file)
    {
        m_real_file = response.file_path;
        if (stat(m_real_file.c_str(), &m_file_stat) < 0)
        {
            return process_write(make_error_response(404, response.keep_alive));
        }
        if (!(m_file_stat.st_mode & S_IROTH))
        {
            return process_write(make_error_response(403, response.keep_alive));
        }
        if (S_ISDIR(m_file_stat.st_mode))
        {
            return process_write(make_error_response(400, response.keep_alive));
        }

        int fd = open(m_real_file.c_str(), O_RDONLY);
        if (fd < 0)
        {
            return false;
        }

        if (m_file_stat.st_size == 0)
        {
            close(fd);
            if (!add_headers(0, response.content_type.c_str()))
            {
                return false;
            }
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv_count = 1;
            bytes_to_send = m_write_idx;
            return true;
        }

        m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (m_file_address == MAP_FAILED)
        {
            m_file_address = NULL;
            return false;
        }

        if (!add_headers(m_file_stat.st_size, response.content_type.c_str()))
        {
            unmap();
            return false;
        }
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;
        bytes_to_send = m_write_idx + m_file_stat.st_size;
        return true;
    }

    if (!add_headers(static_cast<int>(response.body.size()), response.content_type.c_str()))
    {
        return false;
    }
    if (!response.body.empty() && !add_content(response.body.c_str()))
    {
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (m_iv_count == 2 && bytes_have_send >= static_cast<int>(m_iv[0].iov_len))
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            return false;
        }
    }
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len, const char *content_type)
{
    return add_content_length(content_len) && add_content_type(content_type) &&
           add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type(const char *content_type)
{
    return add_response("Content-Type:%s\r\n", content_type);
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", m_linger ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

void http_conn::process()
{
    auto start = std::chrono::steady_clock::now();

    const HttpParser::Result read_ret = process_read();
    if (read_ret == HttpParser::NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }

    HttpResponse response;
    if (read_ret == HttpParser::BAD_REQUEST)
    {
        response = make_error_response(400, false);
    }
    else
    {
        response = build_response();
    }

    const bool write_ret = process_write(response);
    if (!write_ret)
    {
        ServerStats::get_instance().req_failed();
        close_conn();
        return;
    }

    auto end = std::chrono::steady_clock::now();
    ServerStats::get_instance().req_processed(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}
