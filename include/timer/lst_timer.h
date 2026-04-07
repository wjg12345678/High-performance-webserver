#ifndef LST_TIMER
#define LST_TIMER

#include <stdint.h>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
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

#include <time.h>
#include "log/log.h"

class http_conn;
class util_timer;
class SubReactor;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    http_conn *conn;
    util_timer *timer;
    int timer_version;
    SubReactor *reactor;
};

class util_timer
{
public:
    util_timer() : expire(0), cb_func(NULL), user_data(NULL), heap_idx(-1), timer_version(0) {}

public:
    int64_t expire;
    void (* cb_func)(client_data *);
    client_data *user_data;
    int heap_idx;
    int timer_version;
};

class timer_heap
{
public:
    timer_heap();
    ~timer_heap();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();
    int64_t top_expire() const;
    bool empty() const;

private:
    void sift_up(int idx);
    void sift_down(int idx);
    void swap_timer(int lhs, int rhs);
    void remove_at(int idx);

    std::vector<util_timer *> heap_;
};

class Utils
{
public:
    Utils();
    ~Utils();

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    void addsig(int sig, void(handler)(int), bool restart = true);
    bool init_timerfd();
    int get_timerfd() const;
    void update_timerfd();
    bool handle_timer_event();
    int64_t current_time_ms() const;

    void show_error(int connfd, const char *info);

public:
    timer_heap m_timer_heap;
    int m_TIMESLOT;
    int m_timerfd;
};

void cb_func(client_data *user_data);

#endif
