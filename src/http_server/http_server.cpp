#include "http_server.h"
#include "../utils/json.h" // https://github.com/nlohmann/json/tree/develop/single_include/nlohmann/json.hpp
#include <cstddef>
#include <fstream>
#include <memory>

using json = nlohmann::json;
static const std::string CONFIG_FILEPATH = "../conf_http_server.json";

HttpServer::HttpServer(int port, bool is_ET, int timeout, int thread_num)
    : m_port(port)
    , m_is_listen(false)
    , m_timeout(timeout)
    , m_epoll(std::make_unique<Epoll>())
    , m_threadpool(std::make_unique<ThreadPool>(thread_num))
// , timer_(new HeapTimer())
{
    m_src_dir = getcwd(nullptr, 256);
    assert(m_src_dir);
    strncat(m_src_dir, "/resources/", 16);
    HttpConnector::g_user_count = 0;
    HttpConnector::SRC_DIR = m_src_dir;
    HttpConnector::g_is_ET = is_ET;
    //    if(openLog) {
    //        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
    //        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
    //        else {
    //            LOG_INFO("========== Server init ==========");
    //            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
    //            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
    //                     (listenEvent_ & EPOLLET ? "ET": "LT"),
    //                     (connEvent_ & EPOLLET ? "ET": "LT"));
    //            LOG_INFO("LogSys level: %d", logLevel);
    //            LOG_INFO("srcDir: %s", HttpConn::srcDir);
    //            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
    //        }
    //    }
}

bool HttpServer::InitListen()
{
    int ret;
    struct sockaddr_in addr { };
    if (m_port > 65535 || m_port < 1024) {
        //        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);
    struct linger optLinger = { 0 };
    if (openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        //        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(m_listenFd);
        //        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        //        LOG_ERROR("set socket setsockopt error !");
        close(m_listenFd);
        return false;
    }

    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        //        LOG_ERROR("Bind Port:%d error!", port_);
        close(m_listenFd);
        return false;
    }

    ret = listen(m_listenFd, 6);
    if (ret < 0) {
        //        LOG_ERROR("Listen port:%d error!", port_);
        close(m_listenFd);
        return false;
    }
    ret = epoller_->AddFd(m_listenFd, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        //        LOG_ERROR("Add listen error!");
        close(m_listenFd);
        return false;
    }
    SetFdNonblock(m_listenFd);
    //    LOG_INFO("Server port:%d", port_);
    m_is_listen = true;
    return true;
}

bool HttpServer::SetPropertyFromFile(const std::string& path)
{
    std::ifstream fin(path);
    json j;
    fin >> j;
    fin.close();
}