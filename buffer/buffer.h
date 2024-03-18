#ifndef _BUFFER_
#define _BUFFER_

#include <assert.h>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

/*
+-------------------+------------------+------------------+------------------+
| prependable bytes |   readed bytes   |  readable bytes  |  writable bytes  |
+-------------------+------------------+------------------+------------------+
|                   |                  |                  |                  |
0        <=       prePos      <=    readPos     <=     writerPos    <=     size
*/

/**
 * 一个缓冲区，包含，是vector<char>的封装，可以自动扩容,提供prepend空间，让程序能以很低的代价在数据前面添加几个字节
 */
class Buffer {
    // 为什么用裸指针，用智能指针或iterator？-> 因为读数据的iovec结构需要裸指针作为参数
public:
    static const size_t INIT_BUFFER_SIZE = 1024;
    static const size_t INIT_PREPEND_SIZE = 8; // prependable初始大小,即readIndex初始的位置

public:
    explicit Buffer(size_t init_buffer_size = INIT_BUFFER_SIZE)
        : m_buffer(INIT_PREPEND_SIZE + init_buffer_size) // pre_pos是正常情况下的buffer起点
        , m_pre_pos(INIT_PREPEND_SIZE)
        , m_read_pos(INIT_PREPEND_SIZE)
        , m_write_pos(INIT_PREPEND_SIZE) {};
    ~Buffer() = default;
    /**
     * 缓冲区中可写的字节数
     */
    size_t WriteableBytes() const { return m_buffer.size() - m_write_pos; }
    /**
     * 缓冲区中未读的字节数
     */
    size_t ReadableBytes() const { return m_write_pos - m_read_pos; }
    /**
     * 缓冲区中已读过的字节数
     */
    size_t ReadedBytes() const { return m_read_pos - m_pre_pos; }
    /**
     * 返回要预置数据的末尾位置
     */
    char* GetPrePtr() { return GetBeginPtr() + m_pre_pos; }
    const char* GetPrePtr() const { return GetBeginPtr() + m_pre_pos; }
    /**
     * 返回要取出数据的起始位置
     */
    char* GetReadPtr() { return GetBeginPtr() + m_read_pos; }
    const char* GetReadPtr() const { return GetBeginPtr() + m_read_pos; }
    void SetReadPos(char* ptr) { m_read_pos += (ptr - GetReadPtr()); }
    /**
     * 返回可写入数据的起始位置
     */
    char* GetWritePtr() { return GetBeginPtr() + m_write_pos; }
    const char* GetWritePtr() const { return GetBeginPtr() + m_write_pos; }
    void SetWritePos(char* ptr) { m_write_pos += (ptr - GetWritePtr()); }
    /**
     * 从fd向缓冲区中写入数据
     */
    ssize_t ReadFromFd(int fd, int* saved_errno);
    /**
     * 从缓冲区中取出数据到fd
     */
    ssize_t WriteToFd(int fd, int* saved_errno);

private: // 成员函数
    /**
     * 返回缓冲区起始地址
     */
    char* GetBeginPtr() { return &*m_buffer.begin(); }
    const char* GetBeginPtr() const { return &*m_buffer.begin(); }
    /**
     * 判断缓冲区是否够用，不够就创造空间（调用resize函数）
     */
    void EnsureWriteable(size_t len);

private: // 成员变量
    std::atomic<std::size_t> m_pre_pos; // 预置数据的末尾
    std::atomic<std::size_t> m_read_pos; // 已经取出数据的末尾
    std::atomic<std::size_t> m_write_pos; // 已经写入数据的末尾
    std::vector<char> m_buffer; // 缓冲区
};

#endif // !_BUFFER_