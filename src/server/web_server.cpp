#include "server/web_server.h"
#include "server/server_stats.h"

// Global pointer for signal handler to access sub-reactors
WebServer *g_web_server = NULL;

namespace
{
int choose_reactor_count(int reactor_num)
{
    if (reactor_num <= 0)
    {
        return 1;
    }
    return reactor_num;
}

void sig_handler(int)
{
    ServerStats::get_instance().print_stats(g_web_server);
}
} // namespace

WebServer::WebServer() : m_epollfd(-1), m_listenfd(-1), m_pool(NULL), m_next_reactor(0)
{
    users = new http_conn[MAX_FD];

    char server_path[200];
    getcwd(server_path, 200);
    const char root[] = "/resources/www";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    users_timer = new client_data[MAX_FD];
    for (int i = 0; i < MAX_FD; ++i)
    {
        users_timer[i].conn = NULL;
        users_timer[i].timer = NULL;
        users_timer[i].sockfd = -1;
        users_timer[i].timer_version = 0;
        users_timer[i].reactor = NULL;
    }
}

WebServer::~WebServer()
{
    if (m_epollfd != -1)
    {
        close(m_epollfd);
    }
    if (m_listenfd != -1)
    {
        close(m_listenfd);
    }
    for (size_t i = 0; i < m_sub_reactors.size(); ++i)
    {
        delete m_sub_reactors[i];
    }
    delete[] users;
    delete[] users_timer;
    delete m_pool;
    free(m_root);
}

void WebServer::init(int port, string user, string passWord, string databaseName, string dbHost,
                     int dbPort, int log_write,
                     int opt_linger, int trigmode, int sql_num, int thread_num, int reactor_num,
                     int close_log)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_dbHost = dbHost;
    m_dbPort = dbPort;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_reactor_num = reactor_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
}

void WebServer::trig_mode()
{
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        if (1 == m_log_write)
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
        }
    }
}

void WebServer::sql_pool()
{
    m_connPool = connection_pool::GetInstance();
    m_connPool->init(m_dbHost, m_user, m_passWord, m_databaseName, m_dbPort, m_sql_num, m_close_log);
    users->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    m_pool = new threadpool<http_conn>(m_connPool, m_thread_num);
}

void WebServer::eventListen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    Utils utils;
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    utils.addsig(SIGPIPE, SIG_IGN);

    const int reactor_count = choose_reactor_count(m_reactor_num);
    for (int i = 0; i < reactor_count; ++i)
    {
        SubReactor *reactor = new SubReactor(i, m_pool, users, users_timer, m_root,
                                             m_CONNTrigmode, m_close_log,
                                             m_user, m_passWord, m_databaseName);
        assert(reactor->init());
        assert(reactor->start());
        m_sub_reactors.push_back(reactor);
    }
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    while (true)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            if ((m_LISTENTrigmode == 1) && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                break;
            }
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }

        if (http_conn::m_user_count >= MAX_FD)
        {
            Utils utils;
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            continue;
        }

        SubReactor *reactor = m_sub_reactors[m_next_reactor];
        m_next_reactor = (m_next_reactor + 1) % m_sub_reactors.size();
        if (!reactor->add_connection(connfd, client_address))
        {
            close(connfd);
            LOG_ERROR("%s", "dispatch connection failure");
        }

        if (0 == m_LISTENTrigmode)
        {
            break;
        }
    }

    return true;
}

int WebServer::reactor_connections(int i) const
{
    if (i >= 0 && i < static_cast<int>(m_sub_reactors.size()))
    {
        return m_sub_reactors[i]->get_active_connections();
    }
    return 0;
}

int WebServer::threadpool_queue_size() const
{
    if (m_pool)
    {
        return m_pool->queue_size();
    }
    return 0;
}

void WebServer::eventLoop()
{
    g_web_server = this;
    Utils utils;
    utils.addsig(SIGUSR1, sig_handler, false);

    while (true)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "acceptor epoll failure");
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd)
            {
                dealclientdata();
            }
        }
    }
}
