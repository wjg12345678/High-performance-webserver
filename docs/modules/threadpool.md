线程池
======

当前版本线程池只承担业务处理职责，不再区分 reactor/proactor 双模式。

> * `SubReactor` 负责 socket 读写和超时处理
> * 工作线程只处理已经读入完成的 HTTP 请求
> * 线程池任务入口统一为 `append()`

工作流程
--------

- I/O 线程在 `EPOLLIN` 事件中调用 `read_once()`
- 读成功后，将 `http_conn` 对象投递到线程池
- 工作线程从任务队列取出连接对象
- 工作线程持有数据库连接，执行 `http_conn::process()`
- `process()` 完成 HTTP 解析、业务处理与响应构造
- 响应发送由所属 `SubReactor` 在线程中的 `EPOLLOUT` 事件继续完成

这套模型本质上是：

- 主线程作为 `Acceptor`
- 多个 `SubReactor` 处理 I/O
- 一个业务线程池处理请求逻辑






