#include "timer/lst_timer.h"
#include "server/server_stats.h"
#include "server/sub_reactor.h"
#include <cassert>
#include <string.h>

#include "http/http_conn.h"

namespace
{
const int64_t kMillisecondsPerSecond = 1000;

int64_t current_time_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * kMillisecondsPerSecond + ts.tv_nsec / 1000000;
}
}

timer_heap::timer_heap()
{
}

timer_heap::~timer_heap()
{
    for (size_t i = 0; i < heap_.size(); ++i)
    {
        delete heap_[i];
    }
}

void timer_heap::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    timer->heap_idx = static_cast<int>(heap_.size());
    heap_.push_back(timer);
    sift_up(timer->heap_idx);
}

void timer_heap::adjust_timer(util_timer *timer)
{
    if (!timer || timer->heap_idx < 0 || timer->heap_idx >= static_cast<int>(heap_.size()))
    {
        return;
    }
    sift_down(timer->heap_idx);
    sift_up(timer->heap_idx);
}

void timer_heap::del_timer(util_timer *timer)
{
    if (!timer || timer->heap_idx < 0 || timer->heap_idx >= static_cast<int>(heap_.size()))
    {
        return;
    }
    remove_at(timer->heap_idx);
    delete timer;
}

void timer_heap::tick()
{
    const int64_t now = current_time_ms();
    while (!heap_.empty())
    {
        util_timer *timer = heap_.front();
        if (timer->expire > now)
        {
            break;
        }
        remove_at(0);
        if (timer->cb_func)
        {
            timer->cb_func(timer->user_data);
        }
        delete timer;
    }
}

int64_t timer_heap::top_expire() const
{
    if (heap_.empty())
    {
        return -1;
    }
    return heap_.front()->expire;
}

bool timer_heap::empty() const
{
    return heap_.empty();
}

void timer_heap::sift_up(int idx)
{
    while (idx > 0)
    {
        const int parent = (idx - 1) / 2;
        if (heap_[parent]->expire <= heap_[idx]->expire)
        {
            break;
        }
        swap_timer(parent, idx);
        idx = parent;
    }
}

void timer_heap::sift_down(int idx)
{
    const int size = static_cast<int>(heap_.size());
    while (true)
    {
        int smallest = idx;
        const int left = idx * 2 + 1;
        const int right = idx * 2 + 2;
        if (left < size && heap_[left]->expire < heap_[smallest]->expire)
        {
            smallest = left;
        }
        if (right < size && heap_[right]->expire < heap_[smallest]->expire)
        {
            smallest = right;
        }
        if (smallest == idx)
        {
            break;
        }
        swap_timer(idx, smallest);
        idx = smallest;
    }
}

void timer_heap::swap_timer(int lhs, int rhs)
{
    util_timer *tmp = heap_[lhs];
    heap_[lhs] = heap_[rhs];
    heap_[rhs] = tmp;
    heap_[lhs]->heap_idx = lhs;
    heap_[rhs]->heap_idx = rhs;
}

void timer_heap::remove_at(int idx)
{
    const int last = static_cast<int>(heap_.size()) - 1;
    if (idx < 0 || idx > last)
    {
        return;
    }
    if (idx != last)
    {
        swap_timer(idx, last);
    }
    util_timer *timer = heap_.back();
    heap_.pop_back();
    timer->heap_idx = -1;
    if (idx < static_cast<int>(heap_.size()))
    {
        sift_down(idx);
        sift_up(idx);
    }
}

Utils::Utils() : m_TIMESLOT(0), m_timerfd(-1)
{
}

Utils::~Utils()
{
    if (m_timerfd != -1)
    {
        close(m_timerfd);
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if (1 == TRIGMode)
    {
        event.events |= EPOLLET;
    }
    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

bool Utils::init_timerfd()
{
    if (m_timerfd != -1)
    {
        return true;
    }
    m_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    return m_timerfd != -1;
}

int Utils::get_timerfd() const
{
    return m_timerfd;
}

void Utils::update_timerfd()
{
    if (m_timerfd == -1)
    {
        return;
    }

    struct itimerspec new_value;
    memset(&new_value, 0, sizeof(new_value));

    if (!m_timer_heap.empty())
    {
        int64_t diff_ms = m_timer_heap.top_expire() - current_time_ms();
        if (diff_ms < 1)
        {
            diff_ms = 1;
        }
        new_value.it_value.tv_sec = diff_ms / kMillisecondsPerSecond;
        new_value.it_value.tv_nsec = (diff_ms % kMillisecondsPerSecond) * 1000000;
    }

    timerfd_settime(m_timerfd, 0, &new_value, NULL);
}

bool Utils::handle_timer_event()
{
    if (m_timerfd == -1)
    {
        return false;
    }

    uint64_t expirations = 0;
    const ssize_t bytes = read(m_timerfd, &expirations, sizeof(expirations));
    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return false;
        }
        return false;
    }

    m_timer_heap.tick();
    update_timerfd();
    return true;
}

int64_t Utils::current_time_ms() const
{
    return ::current_time_ms();
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void cb_func(client_data *user_data)
{
    assert(user_data);
    if (!user_data->timer || user_data->timer->timer_version != user_data->timer_version)
    {
        return;
    }
    ServerStats::get_instance().conn_timeout();
    if (user_data->reactor)
    {
        user_data->reactor->dec_active_connections();
    }
    if (user_data->conn)
    {
        user_data->conn->close_conn();
    }
}
