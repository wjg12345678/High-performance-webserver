#ifndef SUB_REACTOR_H
#define SUB_REACTOR_H

#include <queue>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>

#include "http/http_conn.h"
#include "threadpool/threadpool.h"

struct pending_conn
{
    int sockfd;
    sockaddr_in address;
};

const int SUB_REACTOR_MAX_EVENT_NUMBER = 10000;

class SubReactor
{
public:
    SubReactor(int id, threadpool<http_conn> *pool, http_conn *users, client_data *users_timer,
               char *root, int conn_trigmode, int close_log,
               string user, string passwd, string database_name);
    ~SubReactor();

    bool init();
    bool start();
    bool add_connection(int connfd, const sockaddr_in &client_address);

private:
    static void *worker(void *arg);
    void run();
    void handle_pending_connections();
    void init_connection(int connfd, const sockaddr_in &client_address);
    void add_timer(int connfd, const sockaddr_in &client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

private:
    int m_id;
    int m_epollfd;
    int m_eventfd;
    bool m_started;
    pthread_t m_thread;
    epoll_event m_events[SUB_REACTOR_MAX_EVENT_NUMBER];

    locker m_pending_lock;
    std::queue<pending_conn> m_pending_connections;

    threadpool<http_conn> *m_pool;
    http_conn *m_users;
    client_data *m_users_timer;
    char *m_root;
    int m_conn_trigmode;
    int m_close_log;
    string m_user;
    string m_passwd;
    string m_database_name;
    Utils m_utils;
};

#endif
