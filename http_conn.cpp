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
int HttpConn::epollfd_ = -1;
int HttpConn::user_count_ = 0;

/* ------------------------------------------------函数定义--------------------------------------------------
 */

void HttpConn::InitConn(int sockfd, const sockaddr_in& addr)
{
    sockfd_ = sockfd;
    address_ = addr;
    // 如下两行是为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉
    int reuse = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    AddFD(epollfd_, sockfd_, true);
    user_count_++;
    Init();
}

void HttpConn::Init()
{
    check_state_ = CHECK_STATE_REQUESTLINE;
    method_ = GET;
    linger_ = false;
    url_ = NULL;
    version_ = NULL;
    content_length_ = 0;
    read_idx_ = 0;
    write_idx_ = 0;
    start_line_ = 0;
    host_ = NULL;
    checked_idx_ = 0;
    memset(read_buf_, 0, READ_BUFFER_SIZE);
    memset(write_buf_, 0, WRITE_BUFFER_SIZE);
    memset(real_file_, 0, FILENAME_LEN);
}

/**
 *
 */
HttpConn::HttpLineStatus HttpConn::ParseLine()
{
    char temp;
    // 遍历行的每一个字符
    for (; checked_idx_ < read_idx_; checked_idx_++) {
        temp = read_buf_[checked_idx_];
        if (temp == '\r') {
            if (checked_idx_ + 1 == read_idx_) {
                return LINE_OPEN;
            } else if (read_buf_[checked_idx_ + 1] == '\n') {
                read_buf_[checked_idx_++] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (checked_idx_ > 1 && read_buf_[checked_idx_ - 1] == '\r') {
                read_buf_[checked_idx_ - 1] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}