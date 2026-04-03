# TinyWebServer

Linux 下的 C++ 轻量级 Web 服务器，使用 `epoll + 线程池 + 定时器 + MySQL 连接池` 实现高并发 HTTP 服务。

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
- `server`：WebServer 核心生命周期、事件循环与连接管理。
- `http`：HTTP 解析、请求处理、响应构造。
- `db`：MySQL 连接池与 RAII 封装。
- `log`：同步/异步日志与阻塞队列。
- `timer`：超时连接定时器与信号处理工具。
- `common`：锁、信号量、条件变量等基础同步原语。
- `resources`：运行时静态文件，不再与源码混放。

## 构建

该项目依赖 Linux 的 `epoll`，需要在 Linux 环境编译运行。

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

再修改 [app/main.cpp](/Users/mac/Desktop/TinyWebServer-master/app/main.cpp) 中的数据库连接信息，然后启动：

```bash
./server
```

支持的命令行参数：

```bash
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
```

## 说明

- 静态资源目录已从原来的 `root/` 调整为 `resources/www/`。
- 源文件列表改为由 `makefile` 自动扫描，不再手工维护。
- 历史说明文档已移动到 `docs/modules/`，便于源码目录保持干净。
