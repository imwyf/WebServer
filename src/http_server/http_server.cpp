#include "http_server.h"
#include "../utils/json.h" // https://github.com/nlohmann/json/tree/develop/single_include/nlohmann/json.hpp
#include "../utils/timer.h"
#include "http_connector.h"
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sys/epoll.h>
using json = nlohmann::json;
static const std::string CONFIG_FILEPATH = "../conf_http_server.json";

HttpServer::HttpServer(int port, int timeout, bool linger, int thread_num,
    bool open_log, int sql_port, const char* sql_user, const char* sql_pwd,
    const char* dbName, int sqlconnpool_num)
    : m_port(port)
    , m_is_listen(false)
    , m_timeout(timeout)
    , m_linger(linger)
    , m_epoll(std::make_unique<Epoll>())
    , m_threadpool(std::make_unique<ThreadPool>(thread_num))
    , m_timer(std::make_unique<Timer>())
{
    m_src_dir = getcwd(nullptr, 256);
    assert(m_src_dir);
    strncat(m_src_dir, "/resources/", 16);
    HttpConnector::g_user_count = 0;
    HttpConnector::SRC_DIR = m_src_dir;
    HttpConnector::g_is_ET = true;
    SqlConnector::GetInstance().InitPool("localhost", sql_port, sql_user, sql_pwd, dbName, sqlconnpool_num);

    if (!InitListen()) {
        m_is_listen = false;
    }

    if (open_log) {
        Log::GetInstance()->Init();
        if (m_is_listen == false) {
            LOG_ERROR("========== Server init error!==========");
        } else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", m_port, m_linger ? "true" : "false");
            LOG_INFO("srcDir: %s", HttpServer::m_src_dir);
            LOG_INFO("Timeout: %d", m_timeout);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", SqlConnector::GetInstance().GetPoolSize(), thread_num);
        }
    }
}

void HttpServer::Start()
{
    int timeout = -1; // epoll wait timeout == -1 无事件将阻塞
    m_is_listen = true; // 启动监听
    if (m_is_listen) {
        LOG_INFO("========== Server start ==========");
    }
    while (m_is_listen) {
        if (m_timeout > 0) {
            timeout = m_timer->GetNextTick(); // 获取下一个事件剩余时间
        }
        int eventCnt = m_epoll->Wait(timeout);
        for (int i = 0; i < eventCnt; i++) { // 根据epoll上的事件，转发至对应方法
            int fd = m_epoll->GetEventFd(i);
            uint32_t events = m_epoll->GetEvents(i);
            if (fd == m_listenFd) { // 新客户端连接
                OnListen();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { // 连接异常
                assert(m_users.count(fd) > 0);
                CloseConn(&m_users[fd]);
            } else if (events & EPOLLIN) { // 客户端发送数据
                assert(m_users.count(fd) > 0);
                OnRead(&m_users[fd]);
            } else if (events & EPOLLOUT) { // 服务器发送数据
                assert(m_users.count(fd) > 0);
                OnWrite(&m_users[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

bool HttpServer::InitListen()
{
    int ret;
    struct sockaddr_in addr { };
    if (m_port > 65535 || m_port < 1024) {
        LOG_ERROR("Port:%d error!", m_port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);
    struct linger linger = { 0 };
    if (m_linger) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        linger.l_onoff = 1;
        linger.l_linger = 1;
    }

    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        LOG_ERROR("Create socket error!", m_port);
        return false;
    }

    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
    if (ret < 0) {
        close(m_listenFd);
        LOG_ERROR("Init linger error!", m_port);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); // 设置linger
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(m_listenFd);
        return false;
    }

    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr)); // 绑定端口
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!", m_port);
        close(m_listenFd);
        return false;
    }

    ret = listen(m_listenFd, 6); // 启动监听
    if (ret < 0) {
        LOG_ERROR("Listen port:%d error!", m_port);
        close(m_listenFd);
        return false;
    }

    ret = m_epoll->AddFd(m_listenFd, EPOLLET | EPOLLIN | EPOLLRDHUP); // listenfd ET模式
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(m_listenFd);
        return false;
    }
    SetFdNonblock(m_listenFd);
    LOG_INFO("Server port:%d", m_port);
    return true;
}

int HttpServer::SetFdNonblock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void HttpServer::SendError(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void HttpServer::CloseConn(HttpConnector* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    m_epoll->DelFd(client->GetFd());
    client->Close();
}

void HttpServer::OnListen()
{
    struct sockaddr_in addr { };
    socklen_t len = sizeof(addr);
    do {
        int connfd = accept(m_listenFd, (struct sockaddr*)&addr, &len); // accept一个connfd
        if (connfd <= 0) {
            return;
        } else if (HttpConnector::g_user_count >= MAX_FD) {
            SendError(connfd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        m_users[connfd].Init(connfd, addr);
        if (m_timeout > 0) {
            // 将新连接添加到定时器中
            m_timer->add(connfd, m_timeout, [this, capture0 = &m_users[connfd]] { CloseConn(capture0); });
        }
        m_epoll->AddFd(connfd, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT); // connfd 也是ET模式
        SetFdNonblock(connfd);
        LOG_INFO("Client[%d] in!", m_users[connfd].GetFd());
    } while (1); // ET模式
}

void HttpServer::OnRead(HttpConnector* client)
{
    assert(client);
    ExtentTime(client);
    /* Reactor：先由主线程负责读写请求\应答报文，然后由工作线程负责处理请求、填充应答 */
    int ret = -1;
    int Errno = 0;
    ret = client->Read(&Errno); // 写入读缓冲
    if (ret <= 0 && Errno != EAGAIN) { // 写入失败
        CloseConn(client);
        return;
    }
    m_threadpool->AddTask([this, client] { OnProcess(client); }); // 写入成功，将任务添加到工作队列，处理请求
}

void HttpServer::OnProcess(HttpConnector* client)
{
    if (client->Process()) { // 处理请求
        m_epoll->ModFd(client->GetFd(), EPOLLET | EPOLLRDHUP | EPOLLONESHOT | EPOLLOUT); // 处理成功就等待写出应答报文
    } else {
        m_epoll->ModFd(client->GetFd(), EPOLLET | EPOLLRDHUP | EPOLLONESHOT | EPOLLIN); // 处理失败就继续等待读取请求报文
    }
}

void HttpServer::OnWrite(HttpConnector* client)
{
    assert(client);
    ExtentTime(client);
    int ret = -1;
    int Errno = 0;
    ret = client->Write(&Errno);
    if (client->ToWriteBytes() == 0) { // 传输完成
        if (client->IsKeepAlive()) { // 长连接就再次处理请求，此时OnProcess会失败导致继续等待读取请求报文
            OnProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (Errno == EAGAIN) { // 写缓冲区满了
            m_epoll->ModFd(client->GetFd(), EPOLLET | EPOLLRDHUP | EPOLLONESHOT | EPOLLOUT); // 继续等待写出
            return;
        }
    }
    CloseConn(client);
}

void HttpServer::ExtentTime(HttpConnector* client)
{
    assert(client);
    // 当连接有新的事件时更新定时器
    if (m_timeout > 0) {
        m_timer->adjust(client->GetFd(), m_timeout);
    }
}

// TODO:
// bool HttpServer::SetPropertyFromFile(const std::string& path)
// {
//     std::ifstream fin(path);
//     json j;
//     fin >> j;
//     fin.close();
// }
