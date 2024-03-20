#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

/**
 * 半同步/半反应堆线程池:内包含一个工作队列，主线程（线程池的创建者）往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。
 * T：封装好的http_connector类，包含了该http连接的所有信息
 */
template <typename T>
class ThreadPool {
public: // 对外暴露的接口
    /**
     * thread_count：线程池中线程的数量
     */
    explicit ThreadPool(size_t thread_count = 8)
        : m_pool(std::make_shared<Pool>())
    {
        assert(thread_count > 0);
        for (size_t i = 0; i < thread_count; i++) { // 创建thread_count个线程
            std::thread([pool = m_pool] { // 使用lambda表达式定义线程执行的run函数
                std::unique_lock<std::mutex> locker(pool->mtx); // 加锁，防止多个线程取到同一个任务
                while (true) { // 死循环
                    if (!pool->task_queue.empty()) { // 若工作队列有任务
                        auto task = std::move(pool->task_queue.front());
                        pool->task_queue.pop();
                        locker.unlock(); // 对任务队列操作结束释放锁
                        task(); // 执行函数
                        locker.lock(); // 函数执行完毕，重新加锁
                    } else if (pool->isClosed)
                        break;
                    else
                        pool->cond.wait(locker); // 线程阻塞等待被唤醒
                }
            })
                .detach();
        }
    }
    ~ThreadPool()
    {
        if (static_cast<bool>(m_pool)) {
            {
                std::lock_guard<std::mutex> locker(m_pool->mtx);
                m_pool->isClosed = true;
            }
            m_pool->cond.notify_all();
        }
    };

    /**
     * 往工作队列中添加任务：先获取锁，添加任务，唤醒一个线程
     */
    void AddTask(T&& task)
    {
        std::lock_guard<std::mutex> locker(m_pool->mtx); // 加入队列时加锁
        m_pool->task_queue.emplace(std::forward<T>(task)); // 将任务添加到队列中
        m_pool->cond.notify_one(); // 唤醒一个线程
    }

private: // 私有类属性
    struct Pool { // 线程池定义，内部包含锁
        std::mutex mtx; // 锁
        std::condition_variable cond; // 条件变量
        bool isClosed = false; // 判断是否关闭线程
        std::queue<std::function<void()>> task_queue; // 任务队列
    };
    std::shared_ptr<Pool> m_pool; // 指向线程池的指针，多个线程共享该内存，因此shared_ptr
};

#endif