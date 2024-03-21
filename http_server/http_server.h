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

    HttpServer(int port, int trigMode, int timeoutMS, bool OptLinger, int threadNum);
    ~HttpServer();
    void Start();

private:
    /* 下面的函数用来配置服务器 */
    bool InitConn();
    void AddClient(int fd, sockaddr_in addr);
    void CloseConn(HttpConnector* client);
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

    int port_;
    bool openLinger_;
    int timeoutMS_; /* 毫秒MS */
    bool isClose_;
    int listenFd_ {};
    char* srcDir_;

    std::unordered_map<int, HttpConnector> m_users;
    std::unique_ptr<Epoll> m_epoll;
    std::unique_ptr<ThreadPool> m_threadpool;
    // std::unique_ptr<HeapTimer> timer_;
};

#endif // _WEBSERVER_H_