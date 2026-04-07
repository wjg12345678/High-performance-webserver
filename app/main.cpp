#include "app/config.h"

int main(int argc, char *argv[])
{
    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT, config.db_user, config.db_password, config.db_name,
                config.db_host, config.db_port, config.LOGWrite,
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num,
                config.reactor_num,
                config.close_log);
    

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}
