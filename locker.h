#ifndef _LOCKER_H_
#define _LOCKER_H_

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/**
 * 对信号量的封装，信号量sem是一个共享资源的引用限制，Wait()阻塞等待sem>0(有资源可用),然后sem-1,代表获取资源，Post()令sem+1，代表释放资源
 */
class Semaphore {
private:
    sem_t sem_; // 信号量结构

public:
    /**
     *   实例化sem信号量
     * @param pshared　0代表线程共享，1代表进程共享
     * @param value　信号量的初始值
     */
    Semaphore(int pshared = 0, int value = 0)
    {
        // RAII-在构造函数中初始化
        if (sem_init(&sem_, pshared, value) != 0) {
            throw std::exception();
        }
    }
    ~Semaphore() { sem_destroy(&sem_); }
    /**
     * 阻塞当前线程直到信号量sem的值大于0,解除阻塞后将sem的值减一，表明获取到公共资源
     * @return True/Fasle 代表是否成功获取资源
     */
    bool Wait() { return sem_wait(&sem_) == 0; }
    /**
     * 增加信号量,释放公共资源
     * @return True/Fasle 代表是否成功释放
     */
    bool Post() { return sem_post(&sem_) == 0; }
};

/**
 *  对互斥锁的封装
 */
class Locker {
private:
    pthread_mutex_t mutex_;

public:
    Locker()
    {
        if (pthread_mutex_init(&mutex_, NULL) != 0) {
            throw std::exception();
        }
    }
    ~Locker() { pthread_mutex_destroy(&mutex_); }
    bool Lock() { return pthread_mutex_lock(&mutex_) == 0; }
    bool Unlock() { return pthread_mutex_unlock(&mutex_) == 0; }
};

/**
 *  封装条件变量
 */
class Condition {
private:
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;

public:
    Condition()
    {
        if (pthread_mutex_init(&mutex_, NULL) != 0) {
            throw std::exception();
        }
        if (pthread_cond_init(&cond_, NULL) != 0) {
            // 构造函数中一旦出现问题，就应该立即释放已经成功分配了的资源
            pthread_mutex_destroy(&mutex_);
            throw std::exception();
        }
    }
    ~Condition()
    {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
    }
    /**
     * 等待条件变量变为真
     */
    bool Wait()
    {
        int ret = 0;
        pthread_mutex_lock(&mutex_);
        ret = pthread_cond_wait(&cond_, &mutex_);
        pthread_mutex_unlock(&mutex_);
        return ret == 0;
    }
    /**
     *  唤醒等待条件变量的线程
     */
    bool Signal() { return pthread_cond_signal(&cond_) == 0; }
};

#endif