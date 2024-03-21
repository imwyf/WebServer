#ifndef _EPOLL_H_
#define _EPOLL_H_

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

class Epoll {
public:
    explicit Epoll(int maxEvent = 1024, bool is_ET = true)
        : m_epfd(epoll_create(512))
        , m_events(maxEvent)
        , m_is_ET(is_ET)
    {
        assert(m_epfd >= 0 && m_events.size() > 0);
    }

    ~Epoll() { close(m_epfd); }

    /**
     * 向epoll事件表注册事件
     */
    bool AddFd(int fd, uint32_t events);
    /**
     * 修改已经注册的fd的监听事件
     */
    bool ModFd(int fd, uint32_t events);
    /**
     * 从epoll事件表中删除一个fd
     */
    bool DelFd(int fd);
    /**
     * 等待epoll上监听的fd产生事件，超时时间timeout，产生的事件需要使用GetEvents获得
     */
    int Wait(int timeout = -1);
    /**
     * 获取产生的事件的来源fd（应在wait之后调用）
     */
    int GetEventFd(size_t i) const;
    /**
     *  获取产生的事件（应在wait之后调用）
     */
    uint32_t GetEvents(size_t i) const;

private:
    bool m_is_ET; // 是否开启ET模式
    int m_epfd; // epoll事件表
    std::vector<struct epoll_event> m_events; // 存储epoll上监听的fd产生的事件
};

#endif // _EPOLL_H_