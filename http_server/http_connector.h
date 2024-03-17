#ifndef _HTTP_CONN_
#define _HTTP_CONN_

#include <arpa/inet.h>
#include <assert.h>
#include <cstddef>
#include <cstring>
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
#include <sys/uio.h>
#include <unistd.h>



/**
 * 封装的连接类，其中包含了与客户端的会话所需的fd，以及处理http请求的函数，作为线程池的模板参数类使用
 */
class HttpConn {
    /* ----------------------------------------------------定义一些http参数--------------------------------------------
     */
public: // 暴露在外常量以及枚举类
    static const int FILENAME_LEN = 200; // 文件名字符数
    static const int READ_BUFFER_SIZE = 2048; // 读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024; // 写缓冲区大小
    HttpConn() { }
    ~HttpConn() { }









public: // 暴露的类属性
    static int m_epollfd; // 所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int m_user_count; // 统计用户数量

public: // 供外部调用的函数
    /**
     * 初始化会话连接，设置参数
     * @param sockfd 会话连接使用的fd
     * @param addr 发方地址
     */
    void InitConn(int sockfd, const sockaddr_in& addr);

    /**
     * 关闭连接
     */
    void CloseConn();

    /**
     * 由线程池中的工作线程调用，这是处理HTTP请求的入口函数，他处理客户发来的http请求报文并填充应答报文至m_iv结构，等待调用Write
     */
    void Process();

    /**
     * 从tcp缓冲(非阻塞)循环读取客户数据到写缓冲中，直到无数据可读或者对方关闭连接
     */
    bool Read();

    /**
     * 向tcp缓冲写入应答报文(非阻塞)
     */
    bool Write();

private: // 内部处理http请求的函数
    /**
     * 初始化一些变量(全0初始化)，并调用构造函数，构造请求对象
     */
    void Init();

    /**
     * 从read_buf中循环取出每一行http语句，并解析http请求
     */
    HttpCode ProcessRead();

    /**
     * 根据服务器处理HTTP请求的结果，填充应答报文进写缓冲中，然后从写缓冲复制到准备发送的m_iv结构中，以供之后调用Write写入tcp缓冲
     */
    bool ProcessWrite(HttpCode ret);

    /* 下面这一组函数被ProcessRead调用以分析HTTP请求
     */

    /**
     * 解析HTTP请求行，获得请求方法、目标URL，以及HTTP版本号
     */
    HttpCode ParseRequestLine(char* text);

    /**
     * 解析HTTP请求的一个头部信息
     */
    HttpCode ParseHeaders(char* text);

    /**
     * 解析请求体：我们没有真正解析HTTP请求的消息体，只是判断它是否被完整地读入了
     */
    HttpCode ParseContent(char* text);

    /**
     * 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性。如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，并告诉调用者获取文件成功
     */
    HttpCode DoRequest();

    /**
     * 获取本行在buffer中的起始位置
     */
    char* GetLine() { return m_read_buf + m_start_line; }

    /**
     * 在read_buf标识出一个完整的行：分析新读入的数据中是否有\r\n，返回LINE_OK表示遇到\r\n，m_checked_idx在新行第一个字符；LINE_OPEN表示没读取到\r\n，m_checked_idx在缓存区末尾，需要继续读取，LINE_BAD表示出错
     */
    HttpLineStatus ParseLine();

    /* 下面这一组函数被ProcessWrite调用以填充HTTP应答
     */

    /**
     * 对内存映射区执行unmap操作
     */
    void Unmap();

    /**
     * 往写缓冲中写入待发送的应答报文
     */
    bool AddResponse(const char* format, ...);

    /**
     * 向应答报文中写入应答体
     */
    bool AddContent(const char* content);

    /**
     * 向应答报文中写入状态行
     */
    bool AddStatusLine(const char* title);

    /**
     * 向应答报文中写入应答头
     */
    bool AddHeaders(int content_length);

    /**
     * 向应答报文中写入应答报文大小
     */
    bool AddContentLength(int content_length);

    /**
     * 向应答报文中写入是否保持长连接
     */
    bool AddLinger();

    /**
     * 向应答报文中写入空行以分割应答头和应答体
     */
    bool AddBlankLine();

private: // 内部使用的类属性
    int m_sockfd; // 该HTTP连接的socket
    sockaddr_in m_address; // 对方的socket地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx; // 标识读缓冲中下一个未读位置
    int m_checked_idx; // 标识读缓冲中正在分析的字符的位置
    int m_start_line; // 当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx; // 写缓冲区中待发送最后一个字节的位置
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