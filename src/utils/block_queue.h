#ifndef _BLOCK_QUEUE_H_
#define _BLOCK_QUEUE_H_

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

template <class T>
class BlockQueue {
public:
    explicit BlockQueue(size_t MaxCapacity = 1000)
        : m_capacity(MaxCapacity)
    {
        assert(MaxCapacity > 0);
        m_is_close = false;
    }

    ~BlockQueue()
    {
        Close();
    }

    void clear();
    bool empty();
    bool full();
    void Close();

    size_t size();
    size_t capacity();

    T front();
    T back();

    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item);
    bool pop(T& item, int timeout);
    void flush();

private:
    std::deque<T> m_deq; // 底层实际数据结构
    size_t m_capacity; // 容量
    std::mutex m_mtx; // 锁
    bool m_is_close; // 状态变量
    std::condition_variable m_cond_consumer; // 消费者条件变量
    std::condition_variable m_cond_producer; // 生产者条件变量
};


template <class T>
void BlockQueue<T>::Close()
{
    {
        std::lock_guard<std::mutex> locker(m_mtx);
        m_deq.clear();
        m_is_close = true;
    }
    m_cond_producer.notify_all();
    m_cond_consumer.notify_all();
};

template <class T>
void BlockQueue<T>::flush()
{
    m_cond_consumer.notify_one();
};

template <class T>
void BlockQueue<T>::clear()
{ 
    std::lock_guard<std::mutex> locker(m_mtx);
    m_deq.clear();
}

template <class T>
T BlockQueue<T>::front()
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_deq.front();
}

template <class T>
T BlockQueue<T>::back()
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_deq.back();
}

template <class T>
size_t BlockQueue<T>::size()
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_deq.size();
}

template <class T>
size_t BlockQueue<T>::capacity()
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_capacity;
}

template <class T>
void BlockQueue<T>::push_back(const T& item)
{ 
    std::unique_lock<std::mutex> locker(m_mtx);
    while (m_deq.size() >= m_capacity) { // 任务数量超过最大容量，暂停生产，等待条件变量
        m_cond_producer.wait(locker);
    }
    m_deq.push_back(item); // 添加任务
    m_cond_consumer.notify_one(); // 通知一个消费者
}

template <class T>
void BlockQueue<T>::push_front(const T& item)
{ 
    std::unique_lock<std::mutex> locker(m_mtx);
    while (m_deq.size() >= m_capacity) {
        m_cond_producer.wait(locker);
    }
    m_deq.push_front(item);
    m_cond_consumer.notify_one();
}

template <class T>
bool BlockQueue<T>::empty()
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_deq.empty();
}

template <class T>
bool BlockQueue<T>::full()
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_deq.size() >= m_capacity;
}

template <class T>
bool BlockQueue<T>::pop(T& item)
{ 
    std::unique_lock<std::mutex> locker(m_mtx);
    while (m_deq.empty()) { // 等待生产者生产，暂停消费
        m_cond_consumer.wait(locker);
        if (m_is_close) {
            return false;
        }
    }
    item = m_deq.front();
    m_deq.pop_front();
    m_cond_producer.notify_one();
    return true;
}

template <class T>
bool BlockQueue<T>::pop(T& item, int timeout)
{
    std::unique_lock<std::mutex> locker(m_mtx);
    while (m_deq.empty()) {
        if (m_cond_consumer.wait_for(locker, std::chrono::seconds(timeout))
            == std::cv_status::timeout) {
            return false;
        }
        if (m_is_close) {
            return false;
        }
    }
    item = m_deq.front();
    m_deq.pop_front();
    m_cond_producer.notify_one();
    return true;
}
#endif //!_BLOCK_QUEUE_H_