#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

// #include "../utils/heap_timer.h"
#include "../utils/threadpool.h"
#include "epoll.h"
#include "http_connector.h"

class HttpServer {
public:
    static const std::string DEFAULTCONFIG_FILEPATH;
    /**
     * 服务器初始化，暂时不启动监听服务，port：监听的端口，is_ET：是否开启ET，timeout：超时器，thread_num：线程池大小
     */
    HttpServer(int port, bool is_ET, int timeout, int thread_num);
    ~HttpServer() { CloseAllConn(); }
    void Start();

private:
    /* 下面的函数用来配置服务器 */

    /**
     * 初始化剪监听服务器
     */
    bool InitListen(bool linger);
    void AddClient(int fd, sockaddr_in addr);
    void CloseConn(HttpConnector* client);
    void CloseAllConn();
    /**
     * 从配置文件读取属性并设置
     */
    bool SetPropertyFromFile(const std::string& path = DEFAULTCONFIG_FILEPATH);

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

    std::unordered_map<int, HttpConnector> m_users;
    std::unique_ptr<Epoll> m_epoll;
    std::unique_ptr<ThreadPool> m_threadpool;
    // std::unique_ptr<HeapTimer> timer_;
};

#endif // _WEBSERVER_H_