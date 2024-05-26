#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include "../utils/log.h"
#include "../utils/threadpool.h"
#include "../utils/timer.h"
#include "epoll.h"
#include "http_connector.h"

class HttpServer {
public:
    static const std::string DEFAULTCONFIG_FILEPATH;
    /**
     * 服务器初始化，暂时不启动监听服务，port：监听的端口，timeout：超时器，thread_num：线程池大小
     */
    HttpServer(int port, int timeout, bool linger, int thread_num, bool open_log, int sql_port, const char* sql_user, const char* sql_pwd, const char* dbName, int sqlconnpool_num);
    ~HttpServer()
    {
        close(m_listenFd);
        m_is_listen = false;
        free(m_src_dir);
        SqlConnector::GetInstance().ClosePool();
    }
    /**
     * 启动服务器，启动监听服务
     */
    void Start();

private:
    /* 下面的函数用来配置服务器 */

    /**
     * 初始化监听服务器
     */
    bool InitListen();
    void AddClient(int fd, sockaddr_in addr);
    void CloseConn(HttpConnector* client);
    /**
     * 从配置文件读取属性并设置
     */
    bool SetPropertyFromFile(const std::string& path = DEFAULTCONFIG_FILEPATH);
    /**
     * 向fd直接发送info
     */
    static void SendError(int fd, const char* info);
    void ExtentTime(HttpConnector* client);

    /* 下面的函数用来处理http */
    void OnListen();
    void OnRead(HttpConnector* client);
    void OnWrite(HttpConnector* client);
    void OnProcess(HttpConnector* client);

    /* 下面的函数用来连接数据库 */

    static const int MAX_FD = 65536;
    static int SetFdNonblock(int fd);

    /* 下面的参数用来控制listenFd */
    int m_port;
    bool m_linger;
    int m_timeout;
    bool m_is_listen;
    int m_listenFd {};
    char* m_src_dir;
    std::atomic<int> g_user_count;

    std::unordered_map<int, HttpConnector> m_users;
    std::unique_ptr<Epoll> m_epoll;
    std::unique_ptr<ThreadPool> m_threadpool;
    std::unique_ptr<Timer> m_timer;
};

#endif // _HTTP_SERVER_H_