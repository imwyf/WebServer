#include "buffer.h"
#include <cstring>

ssize_t Buffer::ReadFromFd(int fd, int* saved_errno)
{
    /*
    在非阻塞网络编程中，如何设计并使用缓冲区？
    1.一方面我们希望减少系统调用，一次读的数据越多越划算，那么似乎应该准备一个大的缓冲区。
    2.另一方面，我们系统减少内存占用。如果有10k个连接，每个连接一建立就分配64k的读缓冲的话，将占用640M内存，而大多数时候这些缓冲区的使用率很低。
    3.因此读取数据的时候要尽可能读完，为了不扩大buffer，可以通过extrabuf来做第二层缓冲
    4.muduo的解决方案：将读取分散在两块区域，一块是buffer，一块是栈上的extrabuf（放置在栈上使得extrabuf随着Read的结束而释放，不会占用额外空间），当初始的小buffer放不下时才扩容
    */
    char extrabuf[65535];

    struct iovec iov[2]; // iovec 有两块
    const size_t writable = WriteableBytes();
    iov[0].iov_base = GetWritePtr(); //  第一块指向m_buffer中的write_pos
    iov[0].iov_len = writable;
    iov[1].iov_base = extrabuf; // 第二块指向栈上的extrabuf
    iov[1].iov_len = sizeof(extrabuf);
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; // 判断需要写入几个缓冲区
    const ssize_t len = readv(fd, iov, iovcnt); // 分散读进两块区域，这样如果读入的数据不多，那么全部都读到m_buffer中；如果长度超过writable，就会读到栈上的extrabuf里
    if (len < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(len) <= writable) { // 判断是否长度超过writable
        m_write_pos += len;
    } else { // 超过就读到栈上的extrabuf里
        m_write_pos = m_buffer.size();
        size_t left = len - writable;
        EnsureWriteable(left); // 给buffer扩容
        memcpy(GetWritePtr(), extrabuf, left); // 最后再把extrabuf的内容读进buffer
        m_write_pos += left;
    }
    return len;
}

ssize_t Buffer::WriteToFd(int fd, int* saved_errno)
{
    const size_t readable = ReadableBytes();
    ssize_t len = write(fd, GetReadPtr(), readable);
    if (len < 0) {
        *saved_errno = errno;
        return len;
    }
    m_read_pos += len;
    return len;
}

void Buffer::EnsureWriteable(size_t len)
{
    /*
    写入空间不够处理方案：
    1.将readable bytes往前移动：因为每次读取数据，readed bytes都会逐渐增大。我们可以将readed bytes直接抛弃，把后面的readable bytes移动到前面prePos处。
    2.如果第一种方案的空间仍然不够，那么就直接对buffer扩容
*/
    if (WriteableBytes() + ReadedBytes() < len) { // 直接扩容
        m_buffer.resize(m_write_pos + len + 1);
    } else { // readable bytes前移
        const size_t readable = ReadableBytes();
        memcpy(GetPrePtr(), GetReadPtr(), readable);
        m_read_pos = INIT_PREPEND_SIZE;
        m_write_pos = m_read_pos + readable;
    }
    assert(WriteableBytes() >= len);
}