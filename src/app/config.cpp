#include "app/config.h"

#include <fstream>
#include <iostream>
using std::cout;
using std::endl;
using std::ifstream;
using std::string;

namespace
{
string trim(const string &value)
{
    const string whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == string::npos)
    {
        return "";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}
}

Config::Config(){
    //端口号,默认9006
    PORT = 9006;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT
    TRIGMode = 0;

    //listenfd触发模式，默认LT
    LISTENTrigmode = 0;

    //connfd触发模式，默认LT
    CONNTrigmode = 0;

    //优雅关闭链接，默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num = 8;

    //线程池内的线程数量,默认8
    thread_num = 8;

    //sub-reactor 数量,默认4
    reactor_num = 4;

    //关闭日志,默认不关闭
    close_log = 0;

    //默认配置文件
    config_file = "config/server.conf";

    //数据库默认配置
    db_host = "127.0.0.1";
    db_port = 3306;
    db_user = "root";
    db_password = "root";
    db_name = "qgydb";
}

bool Config::load_from_file(const char *file_path)
{
    ifstream input(file_path);
    if (!input.is_open())
    {
        return false;
    }

    string line;
    while (std::getline(input, line))
    {
        const size_t comment_pos = line.find('#');
        if (comment_pos != string::npos)
        {
            line = line.substr(0, comment_pos);
        }

        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        const size_t equal_pos = line.find('=');
        if (equal_pos == string::npos)
        {
            continue;
        }

        const string key = trim(line.substr(0, equal_pos));
        const string value = trim(line.substr(equal_pos + 1));

        if (key == "port")
            PORT = atoi(value.c_str());
        else if (key == "log_write")
            LOGWrite = atoi(value.c_str());
        else if (key == "trig_mode")
            TRIGMode = atoi(value.c_str());
        else if (key == "opt_linger")
            OPT_LINGER = atoi(value.c_str());
        else if (key == "sql_num")
            sql_num = atoi(value.c_str());
        else if (key == "thread_num")
            thread_num = atoi(value.c_str());
        else if (key == "reactor_num")
            reactor_num = atoi(value.c_str());
        else if (key == "close_log")
            close_log = atoi(value.c_str());
        else if (key == "db_host")
            db_host = value;
        else if (key == "db_port")
            db_port = atoi(value.c_str());
        else if (key == "db_user")
            db_user = value;
        else if (key == "db_password")
            db_password = value;
        else if (key == "db_name")
            db_name = value;
    }

    return true;
}

bool Config::validate(string &error) const
{
    if (PORT <= 0 || PORT > 65535)
    {
        error = "invalid port, expected 1-65535";
        return false;
    }
    if (LOGWrite != 0 && LOGWrite != 1)
    {
        error = "invalid log_write, expected 0 or 1";
        return false;
    }
    if (TRIGMode < 0 || TRIGMode > 3)
    {
        error = "invalid trig_mode, expected 0-3";
        return false;
    }
    if (OPT_LINGER != 0 && OPT_LINGER != 1)
    {
        error = "invalid opt_linger, expected 0 or 1";
        return false;
    }
    if (sql_num <= 0)
    {
        error = "invalid sql_num, expected > 0";
        return false;
    }
    if (thread_num <= 0)
    {
        error = "invalid thread_num, expected > 0";
        return false;
    }
    if (reactor_num <= 0)
    {
        error = "invalid reactor_num, expected > 0";
        return false;
    }
    if (close_log != 0 && close_log != 1)
    {
        error = "invalid close_log, expected 0 or 1";
        return false;
    }
    if (db_host.empty())
    {
        error = "db_host must not be empty";
        return false;
    }
    if (db_port <= 0 || db_port > 65535)
    {
        error = "invalid db_port, expected 1-65535";
        return false;
    }
    if (db_user.empty())
    {
        error = "db_user must not be empty";
        return false;
    }
    if (db_name.empty())
    {
        error = "db_name must not be empty";
        return false;
    }
    return true;
}

void Config::show_usage(const char *prog_name)
{
    cout << "Usage: " << prog_name
         << " [-f config_file]"
         << " [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER]"
         << " [-s sql_num] [-t thread_num] [-r reactor_num] [-c close_log]" << endl;
    cout << "  -f  config file path" << endl;
    cout << "  -p  listen port" << endl;
    cout << "  -l  log mode: 0 sync, 1 async" << endl;
    cout << "  -m  trigger mode: 0 LT+LT, 1 LT+ET, 2 ET+LT, 3 ET+ET" << endl;
    cout << "  -o  linger option: 0 off, 1 on" << endl;
    cout << "  -s  MySQL connection pool size" << endl;
    cout << "  -t  worker thread pool size" << endl;
    cout << "  -r  sub-reactor count" << endl;
    cout << "  -c  close log: 0 keep, 1 close" << endl;
    cout << "  -h  show this help" << endl;
}

void Config::parse_arg(int argc, char*argv[]){
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            show_usage(argv[0]);
            exit(0);
        }
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
            config_file = argv[i + 1];
            break;
        }
    }

    if (!load_from_file(config_file.c_str()))
    {
        cout << "Failed to load config file: " << config_file << endl;
        exit(1);
    }

    int opt;
    opterr = 0;
    const char *str = ":f:p:l:m:o:s:t:r:c:h";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'f':
        {
            config_file = optarg;
            break;
        }
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOGWrite = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        case 'r':
        {
            reactor_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
        case 'h':
        {
            show_usage(argv[0]);
            exit(0);
        }
        case ':':
        default:
            show_usage(argv[0]);
            exit(1);
            break;
        }
    }

    string error;
    if (!validate(error))
    {
        cout << "Invalid configuration: " << error << endl;
        exit(1);
    }
}
