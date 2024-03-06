#include "http_conn.h"
#include "debug_util.h"
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
    // 解释一下\r\n：CR：CarriageReturn，对应ASCII中转义字符\r，表示回车；LF：Linefeed，对应ASCII中转义字符\n，表示换行；CRLF：CarriageReturn&Linefeed，\r\n，表示回车并换行
    for (; m_checked_idx < m_read_idx; m_checked_idx++) {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') { // 当前的字节是\r，即回车，则说明可能读取到空行
            if (m_checked_idx + 1 == m_read_idx) { // \r字符碰巧是目前buffer中的最后一个已经被读入的数据，那么没法确定，还需要继续读取客户数据才能进一步分析
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') { // CRLF，表示空行，本行结束
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') { // CRLF，本行结束
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

// TODO:DEBUG测试一下，本函数中url、version的指针变化
HttpConn::HttpCode HttpConn::ParseRequestLine(char* text)
{
    m_url = strpbrk(text, "\t"); // 检索字符串 text 中第一个出现的 \t
    if (m_url == NULL) { // 如果请求行中没有空白字符或“\t”字符，则HTTP请求必有问题
        return BAD_REQUEST;
    }
    *m_url = '\0';
    m_url++;

    char* method = text;
    if (strcasecmp(method, "GET") == 0) { // 在字符串 text 中匹配 GET
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, "\t"); // 检索字符串 m_url 中第一个不是 \t 的字符下标（可以理解为upper_bound）
    m_version = strpbrk(m_url, "\t");
    if (m_version == NULL) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, "\t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0) { // 检查url是否合法
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (m_url == NULL || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    DBG("The request URL is:%s\n", m_url);
    m_check_state = CHECK_STATE_HEADER; // HTTP请求行处理完毕，状态转移到请求头的分析
    return NO_REQUEST;
}

HttpConn::HttpCode HttpConn::ParseHeaders(char* text)
{
    if (text[0] == '\0') { // 在ParseLine中，我们会将遇到的 \r\n 置为 \0 ，因此 text 开头是 \0，表示请求头已经解析完毕，不需要重复解析
        if (m_content_length != 0) { // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，状态机转移到CHECK_STATE_CONTENT状态
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST; // 否则说明我们已经得到了一个完整的HTTP请求
    } else if (strncasecmp(text, "Connection:", 11) == 0) { // 处理Connection头部字段
        text += 11;
        text += strspn(text, "\t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) { // 处理Content-Length头部字段
        text += 15;
        text += strspn(text, "\t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) { // 处理Host头部字段
        text += 5;
        text += strspn(text, "\t");
        m_host = text;
    } else {
        printf("oop!unknow header%s\n", text);
    }
    return NO_REQUEST;
}

/* TODO:做真正的解析
 */
HttpConn::HttpCode HttpConn::ParseContent(char* text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx)) { //
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpConn::HttpCode HttpConn::ProcessRead()
{
    // 解释：http中以一个\r\n来分割HTTP语句，因此我们每完成一次读操作，就要调用ParseLine分析新读入的数据中是否有\r\n
    HttpLineStatus line_status = LINE_OK;
    HttpCode ret = NO_REQUEST;
    char* text = 0;

    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) // 主状态机，用于从buffer中循环取出一个完整的行来分析
        || ((line_status = ParseLine()) == LINE_OK)) {
        text = GetLine();
        m_start_line = m_checked_idx; // 设置下一行的起始位置
        DBG("got 1 http line:%s\n", text);
        switch (m_check_state) { // 根据主状态机当前的状态来解析http请求的不同部分
        case CHECK_STATE_REQUESTLINE: {
            ret = ParseRequestLine(text);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER: {
            ret = ParseHeaders(text);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            } else if (ret == GET_REQUEST) {
                return DoRequest();
            }
            break;
        }
        case CHECK_STATE_CONTENT: {
            ret = ParseContent(text);
            if (ret == GET_REQUEST) {
                return DoRequest();
            }
            line_status = LINE_OPEN;
            break;
        }
        default: {
            return INTERNAL_SERVER_ERROR;
        }
        }
    }
    return NO_REQUEST;
}

/*当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性。如果目标文件存
在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，
并告诉调用者获取文件成功*/
HttpConn::HttpCode HttpConn::DoRequest()
{
    strcpy(m_real_file, DOC_ROOT);
    int len = strlen(DOC_ROOT);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0) {
        return NO_RESOURCE;
    }
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
/*对内存映射区执行munmap操作*/
void http_conn::unmap()
{
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
/*写HTTP响应*/
bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while (1) {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            /*如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件。虽然在此期间，服务器无法
            立即接收到同一客户的下一个请求，但这可以保证连接的完整性*/
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send < = bytes_have_send) {
            /*发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接*/
            unmap();
            if (m_linger) {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            } else {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

/*往写缓冲中写入待发送的数据*/
bool http_conn::add_response(const char* format, ...)
{
    if (m_write_idx > = WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len > = (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s%d%s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-
        alive ":" close ");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s", content);
}

/*根据服务器处理HTTP请求的结果，决定返回给客户端的内容*/
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret) {
    case INTERNAL_ERROR: {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form)) {
            return false;
        }
        break;
    }
    case BAD_REQUEST: {
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        if (!add_content(error_400_form)) {
            return false;
        }
        break;
    }
    case NO_RESOURCE: {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form)) {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST: {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form)) {
            return false;
        }
        break;
    }
    case FILE_REQUEST: {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0) {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        } else {
            const char* ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string)) {
                return false;
            }
        }
    }
    default: {
        return false;
    }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}
/*由线程池中的工作线程调用，这是处理HTTP请求的入口函数*/
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}