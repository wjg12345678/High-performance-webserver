http连接处理类
===============

`http_conn` 负责单连接上的 HTTP 解析、业务分发和响应构造。

> * I/O 由所属 `SubReactor` 驱动
> * 业务处理在线程池中执行 `http_conn::process()`
> * 响应通过 `writev` 回写给客户端

当前职责
--------

- 保存连接 socket、读写缓冲区、HTTP 解析状态
- 解析请求行、请求头和消息体
- 处理静态文件请求以及登录/注册逻辑
- 使用 `mmap` 映射静态文件
- 使用 `writev` 组合响应头和文件内容
- 通过 `close_conn()` 统一收口连接关闭和关联状态清理

与旧版本的差异
--------------

- 不再依赖全局单 `epollfd`，而是每个连接绑定所属 `SubReactor` 的 `epollfd`
- 不再维护 reactor 模式下的 `m_state/improv/timer_flag` 状态
- 连接关闭路径与定时器失效逻辑已经统一
