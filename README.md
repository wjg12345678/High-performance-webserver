# TinyWebServer

Linux 下的 C++ 轻量级 Web 服务器，使用 `主从 Reactor + epoll + timerfd + 线程池 + MySQL 连接池` 实现高并发 HTTP 服务。

## 重构后的目录

```text
.
├── app/                    # 进程入口
├── include/                # 对外头文件，按领域分层
│   ├── app/
│   ├── common/
│   ├── db/
│   ├── http/
│   ├── log/
│   ├── server/
│   ├── threadpool/
│   └── timer/
├── src/                    # 源码实现
│   ├── app/
│   ├── db/
│   ├── http/
│   ├── log/
│   ├── server/
│   └── timer/
├── resources/www/          # 静态资源与页面
├── tools/benchmarks/       # 压测工具
├── docs/modules/           # 历史模块说明
├── scripts/                # 构建脚本
├── makefile                # 构建入口
└── server                  # 编译产物
```

## 分层说明

- `app`：程序启动、参数解析、服务装配。
- `server`：Acceptor、SubReactor、连接分发与事件循环。
- `http`：HTTP 解析、请求处理、响应构造。
- `db`：MySQL 连接池与 RAII 封装。
- `log`：同步/异步日志与阻塞队列。
- `timer`：基于 `timerfd + 最小堆` 的超时连接管理。
- `common`：锁、信号量、条件变量等基础同步原语。
- `resources`：运行时静态文件，不再与源码混放。

## 当前并发模型

- 主线程仅负责监听 socket 和 `accept`
- 新连接按 round-robin 分发到多个 `SubReactor`
- 每个 `SubReactor` 持有独立的 `epollfd`、`eventfd`、`timerfd` 和定时器堆
- I/O 线程负责 `read/write/timer`
- 线程池只负责 `http_conn::process()`，不再区分 reactor/proactor 双模式

## 构建

该项目依赖 Linux 的 `epoll`、`eventfd`、`timerfd`，需要在 Linux 环境编译运行。

```bash
./scripts/build.sh
```

或

```bash
make all
```

清理产物：

```bash
make clean
```

## 运行

先准备 MySQL：

```sql
CREATE DATABASE yourdb;
USE yourdb;

CREATE TABLE user (
    username CHAR(50) NULL,
    passwd CHAR(50) NULL
) ENGINE=InnoDB;

INSERT INTO user(username, passwd) VALUES ('name', 'passwd');
```

默认配置文件是 [`config/server.conf`](/Users/mac/Desktop/HpWebServer/config/server.conf)，先按需修改其中的数据库和运行参数，然后启动：

```bash
./server
```

或显式指定配置文件：

```bash
./server -f config/server.conf
```

支持的命令行参数：

```bash
./server [-f config_file] [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-r reactor_num] [-c close_log]
```

参数说明：

- `-f`：配置文件路径，默认 `config/server.conf`
- `-p`：监听端口
- `-l`：日志写入方式，`0` 同步，`1` 异步
- `-m`：触发模式，`0~3` 分别对应 `LT+LT`、`LT+ET`、`ET+LT`、`ET+ET`
- `-o`：`SO_LINGER` 开关
- `-s`：MySQL 连接池数量
- `-t`：业务线程池数量
- `-r`：`SubReactor` 数量
- `-c`：是否关闭日志，`0` 不关闭，`1` 关闭

命令行参数会覆盖配置文件中的同名项。当前配置文件支持：

- `port`
- `log_write`
- `trig_mode`
- `opt_linger`
- `sql_num`
- `thread_num`
- `reactor_num`
- `close_log`
- `db_host`
- `db_port`
- `db_user`
- `db_password`
- `db_name`

启动时会校验关键配置项；例如端口范围、线程数、`reactor_num`、数据库端口和必填数据库字段不合法时，程序会直接报错退出。

## 说明

- 静态资源目录已从原来的 `root/` 调整为 `resources/www/`。
- 源文件列表改为由 `makefile` 自动扫描，不再手工维护。
- 历史说明文档已移动到 `docs/modules/`，便于源码目录保持干净。
- `docs/modules/` 下仍保留部分旧实现说明，和当前主分支代码可能不完全一致，应以源码和本 README 为准。
