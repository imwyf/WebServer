#include "http_connector.h"

const char* HttpConnector::SRC_DIR;
std::atomic<int> HttpConnector::g_user_count;
bool HttpConnector::g_is_ET;

void HttpConnector::Init(int fd, const sockaddr_in& addr)
{
    assert(fd > 0);
    g_user_count++;
    m_addr = addr;
    m_fd = fd;
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
            break;
        }
    } while (g_is_ET);
    return len;
}

ssize_t HttpConnector::Write(int* saveErrno)
{
    ssize_t len = -1;
    do {
        len = writev(m_fd, m_iov, m_iov_cnt);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        if (m_iov[0].iov_len + m_iov[1].iov_len == 0) {
            break;
        } /* 传输结束 */
        else if (static_cast<size_t>(len) > m_iov[0].iov_len) {
            m_iov[1].iov_base = (uint8_t*)m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            if (m_iov[0].iov_len) {
                m_writeBuf.Clear();
                m_iov[0].iov_len = 0;
            }
        } else {
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len;
            m_iov[0].iov_len -= len;
            m_writeBuf.AddReadPos(len);
        }
    } while (g_is_ET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConnector::Process()
{
    m_request.Init();
    if (m_readBuf.ReadableBytes() <= 0) {
        return false;
    } else if (m_request.Parse(m_readBuf)) {
        m_response.Init(SRC_DIR, m_request.GetPath(), m_request.IsKeepAlive(), 200);
    } else {
        m_response.Init(SRC_DIR, m_request.GetPath(), false, 400);
    }

    m_response.MakeResponse(m_writeBuf);
    /* 响应头 */
    m_iov[0].iov_base = const_cast<char*>(m_writeBuf.Peek());
    m_iov[0].iov_len = m_writeBuf.ReadableBytes();
    m_iov_cnt = 1;

    /* 文件 */
    if (m_response.FileLen() > 0 && m_response.File()) {
        m_iov[1].iov_base = m_response.File();
        m_iov[1].iov_len = m_response.FileLen();
        m_iov_cnt = 2;
    }
    return true;
}