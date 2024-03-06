#include "http_conn.h"
#include <cstddef>
#include <cstring>
#include <sys/epoll.h>

/* -----------------------------------------定义HTTP响应的一些状态信息------------------------------------------
 */
const char* OK_200 = "HTTP/1.1 200 OK\r\n";
const char* ERRRO_400_TITLE = "HTTP/1.1 400 Bad Request\r\n";
const char* ERRRO_400_FORM = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* ERRRO_403_TITLE = "HTTP/1.1 403 Forbidden\r\n";
const char* ERRRO_403_FORM = "You do not have permission to get file from this server.\n";
const char* ERRRO_404_TITLE = "HTTP/1.1 404 Not Found\r\n";
const char* ERRRO_404_FORM = "The requested file was not found on this server.\n";
const char* ERRRO_500_TITLE = "HTTP/1.1 500 Internal Server Error\r\n";
const char* ERRRO_500_FORM = "The server has encountered a situation it doesn't know how to handle.\n";

/* -------------------------------------------------定义一些常量-------------------------------------------
 */
const char* VERSION = "HTTP/1.1";
const char* CONTENT_TYPE = "Content-Type: ";
const char* HTML = "text/html; charset=UTF-8";
const char* DOC_ROOT = "/var/www/html/"; // 根目录

/* -------------------------------------------定义一些与epoll有关的函数-----------------------------------------
 */

/**
 * 将fd设置为非阻塞的，返回旧的文件状态
 */
int SetNonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * 将需要监听的fd注册到epoll事件表
 * @param epfd epoll事件表的fd标识
 * @param fd　需要监听的fd
 * @param one_shot　是否开启one_shot
 */
void AddFD(int epfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    SetNonblocking(fd);
}

/**
 * 从epoll事件表中删除fd，并关闭fd
 */
void RemoveFD(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

/**
 * 修改epoll正在监听的fd的需要监听的事件类型
 */
void ModFD(int epfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}

/* --------------------------------------------设置本文件所属的变量的初值----------------------------------------
 */
int HttpConn::m_epollfd = -1;
int HttpConn::m_user_count = 0;

/* ------------------------------------------------函数定义--------------------------------------------------
 */

void HttpConn::InitConn(int sockfd, const sockaddr_in& addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    // 如下两行是为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    AddFD(m_epollfd, m_sockfd, true);
    m_user_count++;
    Init();
}

void HttpConn::Init()
{
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_linger = false;
    m_url = NULL;
    m_version = NULL;
    m_content_length = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_start_line = 0;
    m_host = NULL;
    m_checked_idx = 0;
    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
    memset(m_real_file, 0, FILENAME_LEN);
}

HttpConn::HttpLineStatus HttpConn::ParseLine()
{
    char temp;
    // 遍历行的每一个字符
    /* 解释一下\r\n：
        1.CR：CarriageReturn，对应ASCII中转义字符\r，表示回车；LF：Linefeed，对应ASCII中转义字符\n，表示换行；CRLF：CarriageReturn&Linefeed，\r\n，表示回车并换行
        2.http中以\r\n作为一行的结尾
    */
    for (; m_checked_idx < m_read_idx; m_checked_idx++) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') {
            if (m_checked_idx + 1 == m_read_idx) { // 只是CR，表示用户输入的就是\r，继续读取
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') { // CRLF，表示本行结束
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') { // CRLF，表示本行结束
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

bool HttpConn::Read()
{
    if (m_read_idx >= READ_BUFFER_SIZE) // 读缓冲满了
        return false;

    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // 说明socket缓存中的数据读完了，等待发送方发更多
                return true;
            }
            return false;
        } else if (bytes_read == 0) { // 返回0，说明连接已经断开
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

HttpConn::HttpCode HttpConn::ParseRequestLine(char* text)
{
    m_url = strpbrk(text, "\t");
    if (m_url == NULL) {
        return BAD_REQUEST;
    }
    *m_url = '\0';
    m_url++;

    char* method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, "\t");
    m_version = strpbrk(m_url, "\t");
    if (m_version == NULL) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, "\t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    m_check_state = CHECK_STATE_HEADER;
    return BAD_REQUEST;
}
