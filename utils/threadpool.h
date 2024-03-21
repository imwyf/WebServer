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
 */
class ThreadPool {
public:
    /**
     * thread_count：线程池中线程的数量
     */
    explicit ThreadPool(size_t thread_count = 8)
        : m_stop(false)
    {
        assert(thread_count > 0);
        for (size_t i = 0; i < thread_count; i++) { // 创建thread_count个线程
            m_workers.emplace_back( // 线程加入workers数组
                std::thread( // 新建的线程对象
                    [this] { // 使用lambda表达式定义线程执行的run函数
                        // std::unique_lock<std::mutex> locker(this->m_queue_mutex);
                        while (true) { // 死循环
                            std::function<void()> task;

                            {
                                std::unique_lock<std::mutex> locker(this->m_queue_mutex); // 加锁，RAII风格的锁可以保证当该lambda表达式退出时，解锁
                                this->m_condition.wait(locker,
                                    [this] { return this->m_stop || !this->m_tasks.empty(); }); // 用lambda表达式定义判断任务队列是否有任务的谓词
                                if (this->m_stop && this->m_tasks.empty())
                                    return;
                                task = std::move(this->m_tasks.front()); // move取出任务更高效
                                this->m_tasks.pop();
                            }

                            task(); // 执行任务
                        }

                        // 另一版本的run函数
                        // if (!this->m_tasks.empty()) { // 若工作队列有任务
                        //     auto task = std::move(this->m_tasks.front());
                        //     this->m_tasks.pop();
                        //     locker.unlock(); // 对任务队列操作结束释放锁
                        //     task(); // 执行函数
                        //     locker.lock(); // 函数执行完毕，重新加锁
                        // } else if (this->m_stop)
                        //     break;
                        // else
                        //     this->m_condition.wait(locker); // 线程阻塞等待解锁被唤醒
                    }));
            m_workers.back().detach(); // 线程分离式启动，主线程不用等待子线程结束
        }
    }
    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> locker(m_queue_mutex);
            m_stop = true;
        }
        m_condition.notify_all();
    };

    /**
     * 往工作队列中添加任务，唤醒一个线程，task应是一个右值，匿名函数，lambda表达式
     */
    template <typename T>
    void AddTask(T&& task)
    {
        {
            std::lock_guard<std::mutex> locker(m_queue_mutex); // 加入队列时加锁
            m_tasks.emplace(std::forward<T>(task)); // 将任务添加到队列中
        }
        m_condition.notify_one(); // 唤醒一个线程
    }

private:
    std::vector<std::thread> m_workers; // 线程数组
    std::queue<std::function<void()>> m_tasks; // 任务队列

    std::mutex m_queue_mutex; // 互斥量
    std::condition_variable m_condition; // 条件变量
    bool m_stop; // 停止或开始任务
};

#endif