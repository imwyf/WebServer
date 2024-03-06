#ifndef _HTTP_CONN_
#define _HTTP_CONN_

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * 封装的请求类，作为线程池的模板参数类使用
 */
class HttpConn {
    /* ----------------------------------------------------定义一些http参数--------------------------------------------
     */
public:
    static const int FILENAME_LEN = 200; // 文件名字符数
    static const int READ_BUFFER_SIZE = 2048; // 读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024; // 写缓冲区大小
    HttpConn() { }
    ~HttpConn() { }

    enum HttpMethod {
        GET,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

    enum HttpCheckState {
        CHECK_STATE_REQUESTLINE,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum HttpCode {
        OK = 200,
        BAD_REQUEST = 400,
        NOT_FOUND = 404,
        INTERNAL_SERVER_ERROR = 500
    };

    enum HttpVersion {
        HTTP_1_0 = 10,
        HTTP_1_1 = 11
    };

    enum HttpLineStatus {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    /* -------------------------------------------------处理http请求的函数-------------------------------------------
     */
public:
    /**
     * 初始化请求，设置参数
     * @param sockfd 从哪个fd收到该请求
     * @param addr 发方地址
     */
    void InitConn(int sockfd, const sockaddr_in& addr);

    /**
     *
     */
    void CloseConn(bool real_close);
    void Process(); // 处理请求

    /**
     * (非阻塞)循环读取客户数据，直到无数据可读或者对方关闭连接
     */
    bool Read();

    /**
     * 写入应答报文(非阻塞)
     */
    bool Write();

private:
    /**
     * 初始化一些变量(全0初始化)，并调用构造函数，构造请求对象
     */
    void Init();

    HttpCode ProcessRead(); // 解析http请求
    bool ProcessWrite(HttpCode ret); // 填充http应答

    /* 下面这一组函数被ProcessRead调用以分析HTTP请求 */

    /**
     * 解析HTTP请求行，获得请求方法、目标URL，以及HTTP版本号
     */
    HttpCode ParseRequestLine(char* text);

    HttpCode ParseHeaders(char* text);
    HttpCode ParseContent(char* text);
    HttpCode DoRequest();
    char* GetLine() { return m_read_buf + m_start_line; }
    HttpLineStatus ParseLine();

    /* 下面这一组函数被ProcessWrite调用以填充HTTP应答 */
    void Unmap();
    bool AddResponse(const char* format, ...);
    bool AddContent(const char* content);
    bool AddStatusLine(int status, const char* title);
    bool AddHeaders(int content_length);
    bool AddContentLength(int content_length);
    bool AddLinger();
    bool AddBlankLine();

public:
    static int m_epollfd; // 所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int m_user_count; // 统计用户数量

private:
    int m_sockfd; // 该HTTP连接的socket
    sockaddr_in m_address; // 对方的socket地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx; // 标识读缓冲中下一个未读位置
    int m_checked_idx; // 标识读缓冲中正在分析的字符的位置
    int m_start_line; // 当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx; // 写缓冲区中待发送的字节数
    HttpCheckState m_check_state; // 主状态机当前所处的状态
    HttpMethod m_method; // 请求方法
    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于doc_root+url_，doc_root是网站根目录
    char* m_url; // 客户请求的目标文件的文件名
    char* m_version; // HTTP协议版本号，我们仅支持HTTP/1.1
    char* m_host; // 主机名
    int m_content_length; // HTTP请求的消息体的长度
    bool m_linger; // HTTP请求是否要求保持连接
    char* m_file_address; // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat; // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2]; // writev写入的参数
    int m_iv_count; // 被写内存块的数量
};

#endif