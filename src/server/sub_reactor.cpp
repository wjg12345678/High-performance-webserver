#include "server/sub_reactor.h"

#include <cassert>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

namespace
{
const int kTimeslot = 5;
const int kTimerTimeoutMs = 3 * kTimeslot * 1000;
}

SubReactor::SubReactor(int id, threadpool<http_conn> *pool, http_conn *users, client_data *users_timer,
                       char *root, int conn_trigmode, int close_log,
                       string user, string passwd, string database_name)
    : m_id(id),
      m_epollfd(-1),
      m_eventfd(-1),
      m_started(false),
      m_pool(pool),
      m_users(users),
      m_users_timer(users_timer),
      m_root(root),
      m_conn_trigmode(conn_trigmode),
      m_close_log(close_log),
      m_user(user),
      m_passwd(passwd),
      m_database_name(database_name)
{
}

SubReactor::~SubReactor()
{
    if (m_eventfd != -1)
    {
        close(m_eventfd);
    }
    if (m_epollfd != -1)
    {
        close(m_epollfd);
    }
}

bool SubReactor::init()
{
    m_utils.init(kTimeslot);
    m_epollfd = epoll_create(5);
    if (m_epollfd == -1)
    {
        return false;
    }
    if (!m_utils.init_timerfd())
    {
        return false;
    }

    m_eventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_eventfd == -1)
    {
        return false;
    }

    m_utils.addfd(m_epollfd, m_eventfd, false, 0);
    m_utils.addfd(m_epollfd, m_utils.get_timerfd(), false, 0);
    return true;
}

bool SubReactor::start()
{
    if (m_started)
    {
        return true;
    }
    if (pthread_create(&m_thread, NULL, worker, this) != 0)
    {
        return false;
    }
    if (pthread_detach(m_thread) != 0)
    {
        return false;
    }
    m_started = true;
    return true;
}

bool SubReactor::add_connection(int connfd, const sockaddr_in &client_address)
{
    m_pending_lock.lock();
    pending_conn conn;
    conn.sockfd = connfd;
    conn.address = client_address;
    m_pending_connections.push(conn);
    m_pending_lock.unlock();

    uint64_t one = 1;
    return write(m_eventfd, &one, sizeof(one)) == sizeof(one);
}

void *SubReactor::worker(void *arg)
{
    SubReactor *reactor = static_cast<SubReactor *>(arg);
    reactor->run();
    return reactor;
}

void SubReactor::run()
{
    while (true)
    {
        int number = epoll_wait(m_epollfd, m_events, SUB_REACTOR_MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("sub reactor %d epoll failure", m_id);
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            const int sockfd = m_events[i].data.fd;
            if (sockfd == m_eventfd && (m_events[i].events & EPOLLIN))
            {
                uint64_t value = 0;
                read(m_eventfd, &value, sizeof(value));
                handle_pending_connections();
            }
            else if (sockfd == m_utils.get_timerfd() && (m_events[i].events & EPOLLIN))
            {
                if (!m_utils.handle_timer_event())
                {
                    LOG_ERROR("sub reactor %d handle timer failure", m_id);
                }
            }
            else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                deal_timer(m_users_timer[sockfd].timer, sockfd);
            }
            else if (m_events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (m_events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
    }
}

void SubReactor::handle_pending_connections()
{
    while (true)
    {
        m_pending_lock.lock();
        if (m_pending_connections.empty())
        {
            m_pending_lock.unlock();
            break;
        }
        pending_conn conn = m_pending_connections.front();
        m_pending_connections.pop();
        m_pending_lock.unlock();
        init_connection(conn.sockfd, conn.address);
    }
}

void SubReactor::init_connection(int connfd, const sockaddr_in &client_address)
{
    m_users[connfd].init(connfd, client_address, m_root, m_conn_trigmode, m_close_log,
                         m_user, m_passwd, m_database_name, m_epollfd);
    add_timer(connfd, client_address);
}

void SubReactor::add_timer(int connfd, const sockaddr_in &client_address)
{
    m_users_timer[connfd].address = client_address;
    m_users_timer[connfd].sockfd = connfd;
    m_users_timer[connfd].conn = &m_users[connfd];
    ++m_users_timer[connfd].timer_version;

    util_timer *timer = new util_timer;
    timer->user_data = &m_users_timer[connfd];
    timer->cb_func = cb_func;
    timer->expire = m_utils.current_time_ms() + kTimerTimeoutMs;
    timer->timer_version = m_users_timer[connfd].timer_version;
    m_users_timer[connfd].timer = timer;
    m_users[connfd].bind_client_data(&m_users_timer[connfd]);
    m_utils.m_timer_heap.add_timer(timer);
    m_utils.update_timerfd();
}

void SubReactor::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    timer->expire = m_utils.current_time_ms() + kTimerTimeoutMs;
    m_utils.m_timer_heap.adjust_timer(timer);
    m_utils.update_timerfd();
}

void SubReactor::deal_timer(util_timer *timer, int sockfd)
{
    if (!timer)
    {
        m_users[sockfd].close_conn();
        return;
    }

    m_users[sockfd].close_conn();
    m_utils.m_timer_heap.del_timer(timer);
    m_utils.update_timerfd();
}

void SubReactor::dealwithread(int sockfd)
{
    util_timer *timer = m_users_timer[sockfd].timer;
    if (m_users[sockfd].read_once())
    {
        if (m_pool->append(m_users + sockfd))
        {
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
    else
    {
        deal_timer(timer, sockfd);
    }
}

void SubReactor::dealwithwrite(int sockfd)
{
    util_timer *timer = m_users_timer[sockfd].timer;
    if (m_users[sockfd].write())
    {
        if (timer)
        {
            adjust_timer(timer);
        }
    }
    else
    {
        deal_timer(timer, sockfd);
    }
}
