#ifndef _HTTP_CONNECTOR_H
#define _HTTP_CONNECTOR_H

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <sys/uio.h>

#include "../buffer/buffer.h"
#include "http_request.h"
#include "http_response.h"

/*
 * HttpConnector对象构造即初始化，析构时自动Close
 */

/**
 * http连接的封装类，其包含一对读写缓冲区，和用于解析http请求报文、填充应答报文的的HttpRequest、HttpResponse模块，为了实现buffer与上述两个模块的解耦，并未将buffer嵌入模块中，
 * 而是作为HttpConnector的模块与两个http解析模块平行。此类与应作为线程池的工作队列中使用的模板类使用，即task本身。
 */
class HttpConnector {
public:
    /**
     * 构造方法，应传递connfd，以及客户端addr作为参数
     */
    HttpConnector() { }
    ~HttpConnector() { Close(); }
    void Init(int sockFd, const sockaddr_in& addr);
    /**
     * 从本连接的fd向读缓冲写入
     */
    ssize_t Read(int* saveErrno);
    /**
     * 从写缓冲向本连接的fd写出
     */
    ssize_t Write(int* saveErrno);

    /**
     * 关闭socket连接
     */
    void Close();
    int GetFd() const { return m_fd; }
    int GetPort() const { return m_addr.sin_port; }

    const char* GetIP() const { return inet_ntoa(m_addr.sin_addr); }

    sockaddr_in GetAddr() const { return m_addr; }

    /**
     * 处理事务的入口函数
     */
    bool Process();

    /**
     * 返回需要写出的字节数
     */
    int ToWriteBytes() { return m_iov[0].iov_len + m_iov[1].iov_len; }

    bool IsKeepAlive() const { return m_request.IsKeepAlive(); }

    static bool g_is_ET;
    static const char* SRC_DIR;
    static std::atomic<int> g_user_count;

private:
    int m_fd;
    struct sockaddr_in m_addr;

    bool m_is_close;

    int m_iov_cnt {};
    struct iovec m_iov[2] {};

    Buffer m_readBuf; // 读缓冲区
    Buffer m_writeBuf; // 写缓冲区

    HttpRequest m_request;
    HttpResponse m_response;
};

#endif // _HTTP_CONNECTOR_H