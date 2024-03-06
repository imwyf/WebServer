#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include "locker.h"
#include <cstdio>
#include <exception>
#include <list>
#include <pthread.h>
#include <vector>

/**
 * @brief 半同步/半反应堆线程池:内包含一个工作队列，主线程（线程池的创建者）往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。
 *
 * @tparam T:封装好的请求类
 */
template <typename T>
class ThreadPool {
public:
    /**
     * @param threads_num 线程池中线程的数量
     * @param max_request 请求队列容量
     */
    ThreadPool(size_t threads_num = 8, size_t max_request = 1000)
        : m_thread_num(threads_num)
        , m_max_request(max_request)
    {
        if (threads_num <= 0 || max_request <= 0) {
            throw std::exception();
        }
        m_request_queue = new std::list<T*>(m_max_request);
        m_threadpool = new pthread_t[m_thread_num];
        /* 创建thread_number个线程，并将它们都设置为脱离线程 */
        for (int i = 0; i < m_thread_num; ++i) {
            printf("create the %d-th thread\n", i);
            if (pthread_create(m_threadpool + i, NULL, Worker, this) != 0) { // this就是主线程（线程池的创建者）
                delete m_threadpool;
                throw std::exception();
            }
            if (pthread_detach(*(m_threadpool + i))) {
                delete m_threadpool;
                throw std::exception();
            }
        }
    };
    ~ThreadPool()
    {
        delete m_threadpool;
        m_is_stop = true;
    };

    /**
     * @brief 往请求队列中添加任务：先获取锁，然后检查队列容量，最后添加任务并释放锁and信号量+1
     */
    bool Append(T* request)
    {
        m_locker.Lock();
        if (m_request_queue.size() > m_max_request) {
            m_locker.Unlock();
            return false;
        } else {
            m_request_queue.push_back(request);
            m_locker.Unlock();
            m_sem.Post();
            return true;
        }
    }

private:
    /**
     * @brief 工作线程运行的函数，它调用传入线程池的run()：不断从工作队列中取出任务并执行之
     *
     * @param arg:应当传入线程池的指针ThreadPool*
     * @return void*：run()之后的线程池
     */
    static void* Worker(void* arg)
    {
        ThreadPool* pool = static_cast<ThreadPool*>(arg);
        pool->run();
        return pool;
    }

    /**
     * @brief 直到线程结束为止，一直不断从工作队列中取出任务并执行之
     *
     */
    void run()
    {
        while (!m_is_stop) {
            m_sem.Wait();
            m_locker.Lock();
            if (m_request_queue.empty()) {
                m_locker.Unlock();
                continue;
            } else {
                // 工作队列不为空，则取出一个执行
                T* request = m_request_queue.front();
                m_request_queue.pop_front();
                m_locker.Unlock();
                // 执行任务
                if (request) {
                    request->Process();
                }
            }
        }
    }

private:
    int m_thread_num; // 线程池中的线程数
    int m_max_request; // 请求队列容量
    pthread_t* m_threadpool; // 描述线程池的数组
    std::list<T*> m_request_queue; // 请求队列 // TODO:请求为什么不直接放在list，而是存指针
    Locker m_locker; // 保护请求队列的互斥锁
    Semaphore m_sem; // 待处理任务数作为信号量
    bool m_is_stop; // 是否结束线程
};

#endif