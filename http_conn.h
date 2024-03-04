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
    /* ----------------------------------------------------定义一些http参数-------------------------------------------- */
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

    /* -------------------------------------------------处理http请求的函数------------------------------------------- */
public:
    /**
     * 初始化请求
     * @param sockfd 从哪个fd收到该请求
     * @param addr 发方地址
     */
    void InitConn(int sockfd, const sockaddr_in& addr);

    /**
     *
     */
    void CloseConn(bool real_close);
    void Process(); // 处理请求
    bool Read(); // 读取请求报文(非阻塞)
    bool Write(); // 写入应答报文(非阻塞)

private:
    void Init();
    HttpCode ProcessRead(); // 解析http请求
    bool ProcessWrite(HttpCode ret); // 填充http应答

    /* 下面这一组函数被process_read调用以分析HTTP请求 */
    HttpCode ParseRequestLine(char* text);
    HttpCode ParseHeaders(char* text);
    HttpCode ParseContent(char* text);
    HttpCode DoRequest();
    char* GetLine() { return read_buf_ + start_line_; }
    HttpLineStatus ParseLine();

    /* 下面这一组函数被process_write调用以填充HTTP应答 */
    void Unmap();
    bool AddResponse(const char* format, ...);
    bool AddContent(const char* content);
    bool AddStatusLine(int status, const char* title);
    bool AddHeaders(int content_length);
    bool AddContentLength(int content_length);
    bool AddLinger();
    bool AddBlankLine();

public:
    static int epollfd_; // 所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int user_count_; // 统计用户数量

private:
    int sockfd_; // 该HTTP连接的socket
    sockaddr_in address_; // 对方的socket地址
    char read_buf_[READ_BUFFER_SIZE]; // 读缓冲区
    int read_idx_; // 标识读缓冲中下一个未读位置
    int checked_idx_; // 标识读缓冲中正在分析的字符的位置
    int start_line_; // 当前正在解析的行的起始位置
    char write_buf_[WRITE_BUFFER_SIZE]; // 写缓冲区
    int write_idx_; // 写缓冲区中待发送的字节数
    HttpCheckState check_state_; // 主状态机当前所处的状态
    HttpMethod method_; // 请求方法
    char real_file_[FILENAME_LEN]; // 客户请求的目标文件的完整路径，其内容等于doc_root+url_，doc_root是网站根目录
    char* url_; // 客户请求的目标文件的文件名
    char* version_; // HTTP协议版本号，我们仅支持HTTP/1.1
    char* host_; // 主机名
    int content_length_; // HTTP请求的消息体的长度
    bool linger_; // HTTP请求是否要求保持连接
    char* file_address_; // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat file_stat_; // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec iv_[2]; // writev写入的参数
    int iv_count_; // 被写内存块的数量
};

#endif