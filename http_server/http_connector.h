#ifndef _HTTP_CONNECTOR_H
#define _HTTP_CONNECTOR_H

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/uio.h>

#include "../buffer/buffer.h"
#include "http_request.h"
#include "http_response.h"

class HttpConnector {
public:
    HttpConnector(int sockFd, const sockaddr_in& addr) { Init(sockFd, addr); }
    ~HttpConnector() { Close(); }
    void Init(int sockFd, const sockaddr_in& addr);

    /**
     * 从本连接的fd向读缓冲写入
     */
    ssize_t Read(int* saveErrno);
    /**
     * 从写缓冲向本连接的fd写入
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