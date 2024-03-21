
#include "epoll.h"

bool Epoll::AddFd(int fd, uint32_t events)
{
    if (fd < 0)
        return false;
    epoll_event ev = { 0 };
    ev.data.fd = fd;
    ev.events = events;
    ev.events = m_is_ET ? EPOLLIN | EPOLLET : EPOLLIN;
    return epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Epoll::ModFd(int fd, uint32_t events)
{
    if (fd < 0)
        return false;
    epoll_event ev = { 0 };
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoll::DelFd(int fd)
{
    if (fd < 0)
        return false;
    epoll_event ev = { 0 };
    return epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, &ev) == 0;
}

int Epoll::Wait(int timeout)
{
    return epoll_wait(m_epfd, &m_events[0], static_cast<int>(m_events.size()), timeout);
}

int Epoll::GetEventFd(size_t i) const
{
    assert(i < m_events.size() && i >= 0);
    return m_events[i].data.fd;
}

uint32_t Epoll::GetEvents(size_t i) const
{
    assert(i < m_events.size() && i >= 0);
    return m_events[i].events;
}