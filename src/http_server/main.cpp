#include "../utils/locker.h"
#include "../utils/threadpool.h"
#include "http_connector.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/socket.h>

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
extern int AddFD(int epollfd, int fd, bool one_shot);

/**
 * 调用sigaction系统调用，为信号sig注册处理函数handler，参数restart设置是否在handler执行过程中屏蔽其他信号
 */
void AddSig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

/**
 * 向connfd发送消息info，并在控制台打印info
 */
void ShowError(int connfd, const char* info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

/**
 * WebServer服务的启动入口函数，应该被编译名为WebServer，参数为ip和port
 */
int main(int argc, char* argv[])
{
    if (argc <= 2) {
        printf("usage:%s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    AddSig(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
    ThreadPool<HttpConn>* pool = NULL; // 创建线程池
    try {
        pool = new ThreadPool<HttpConn>;
    } catch (...) {
        return 1;
    }
    HttpConn* users = new HttpConn[MAX_FD]; // 预先为每个可能的客户连接分配一个HttpConn对象
    assert(users);
    int user_count = 0;
    int listenfd = socket(PF_INET, SOCK_STREAM, 0); // 创建监听socketfd
    assert(listenfd >= 0);
    struct linger tmp = { 1, 0 };
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp)); // 设置linger为0
    int ret = 0;
    struct sockaddr_in address; // 创建一个地址
    bzero(&address, sizeof(address)); // 初始化该地址为0
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr); // 设置该地址的ip
    address.sin_port = htons(port); // 设置该地址的port
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address)); // 绑定监听fd到该地址
    assert(ret >= 0);
    ret = listen(listenfd, 5); // 监听，5代表已连接队列中最多存放5个待accpet的连接
    assert(ret >= 0);
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5); // 创建epoll内核事件表
    assert(epollfd != -1);
    AddFD(epollfd, listenfd, false); // 添加监听fd
    HttpConn::m_epollfd = epollfd;
    while (true) { // 死循环，用来接受连接以及处理请求、回复应答报文
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); // 等待epoll监听的fd上产生事件，产生的事件存储在events中，返回的number是产生了几个事件
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++) { // 依次处理这些事件
            int sockfd = events[i].data.fd; // 先查该事件从哪个fd触发的
            if (sockfd == listenfd) { // 若是监听fd，说明是客户端发来的连接请求(即客户端调用connect()，向服务器发送一个只包含ISN的tcp段)
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength); // accept会从已连接队列中取出一个连接，读取ISN和客户端的地址，并以此构建一个新的connfd作为会话fd，(同时应该也会回一个确认报文给客户端)
                if (connfd < 0) { // accept出错
                    printf("errno is:%d\n", errno);
                    continue;
                }
                if (HttpConn::m_user_count >= MAX_FD) { // 服务器所有可使用的fd已满，无法再新建fd
                    ShowError(connfd, "Inter nal server busy"); // 向客户端回一个应答报文
                    continue;
                }
                users[connfd].InitConn(connfd, client_address); // 初始化客户连接
            } else if (events[i].events & EPOLLIN) { // 若不是监听fd，说明是从已连接的会话fd(connfd)发来的正常客户端请求
                if (users[sockfd].Read()) { // 读请求
                    pool->Append(users + sockfd); // 将该连接加入请求队列，其他的线程会竞争的获取该请求并执行(执行过程见HttpConn::Process)
                } else {
                    users[sockfd].CloseConn(); // 读取失败，关闭连接
                }
            } else if (events[i].events & EPOLLOUT) { // 若是写事件，说明服务器要回复一个应答报文
                if (!users[sockfd].Write()) { // 根据写的结果，决定是否关闭连接
                    users[sockfd].CloseConn();
                }
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { // 如果有异常，直接关闭客户连接
                users[sockfd].CloseConn();
            } else {
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
