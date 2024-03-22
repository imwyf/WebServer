#include "http_connector.h"

static bool g_is_ET = true;

void HttpConnector::Reset(int sockfd, const sockaddr_in& addr)
{
    assert(sockfd > 0);
    g_user_count++;
    m_addr = addr;
    m_fd = sockfd;
    m_writeBuf.Clear();
    m_readBuf.Clear();
    m_is_close = false;
}

void HttpConnector::Close()
{
    m_response.UnmapFile(); // 取消映射
    if (!m_is_close) {
        m_is_close = true;
        g_user_count--; // 减少用户
        close(m_fd);
    }
}

ssize_t HttpConnector::Read(int* saveErrno)
{
    ssize_t len = -1;
    do {
        len = m_readBuf.ReadFromFd(m_fd, saveErrno);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
    } while (g_is_ET); // TODO:分析ET的设计
    return len;
}

ssize_t HttpConnector::Write(int* saveErrno)
{
    ssize_t len = -1;
    do {
        /* m_iov[0]（写缓冲）存放状态行、头部字段和空行，m_iov[1]存放文档内容即应答体 */
        len = writev(m_fd, m_iov, m_iov_cnt); // writev函数以顺序iov[0]、iov[1]至iov[iovcnt-1]从各缓冲区中聚集输出数据到fd，称为集中写
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        if (m_iov[0].iov_len + m_iov[1].iov_len == 0) { // 写完成
            break;
        } else if (static_cast<size_t>(len) > m_iov[0].iov_len) { // 存在应答体
            /* 下面是直接更新iov内存块的指针，以及写缓冲的指针，表示已经从写缓冲写入fd */
            m_iov[1].iov_base = (uint8_t*)m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            if (m_iov[0].iov_len) {
                m_writeBuf.Clear();
                m_iov[0].iov_len = 0;
            }
        } else { // 不存在应答体
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len;
            m_iov[0].iov_len -= len;
            m_writeBuf.AddReadPos(len);
        }
    } while (g_is_ET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConnector::Process()
{
    m_request.Reset(); // process之前，先重置用来保存请求报文属性的m_request
    if (m_readBuf.ReadableBytes() <= 0) {
        return false;
    }

    if (m_request.Parse(m_readBuf)) { // 先解析读缓冲的请求报文，然后根据其内容重置用来写入应答报文的m_response
        m_response.Reset(SRC_DIR, m_request.GetPath(), m_request.IsKeepAlive(), 200);
    } else {
        m_response.Reset(SRC_DIR, m_request.GetPath(), false, 400);
    }

    m_response.MakeResponse(m_writeBuf);
    /* 状态行、头部字段和空行放在写缓冲 */
    m_iov[0].iov_base = const_cast<char*>(m_writeBuf.GetReadPtr());
    m_iov[0].iov_len = m_writeBuf.ReadableBytes();
    m_iov_cnt = 1;

    /* 文件内容放在另一块内存 */
    if (m_response.FileLen() > 0 && m_response.GetFile()) {
        m_iov[1].iov_base = m_response.GetFile();
        m_iov[1].iov_len = m_response.FileLen();
        m_iov_cnt = 2;
    }
    return true;
}