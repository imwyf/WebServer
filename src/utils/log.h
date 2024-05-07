#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "../buffer/buffer.h"
#include "block_queue.h"
#include <assert.h>
#include <mutex>
#include <stdarg.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>

class Log {
public:
    static Log* GetInstance() // 懒汉单例
    {
        static Log instance;
        return &instance;
    }

    /**
     * 初始化日志实例（阻塞队列最大容量、日志保存路径、日志文件后缀）
     */
    void Init(int max_queue_capacity = 1024,
        const char* path = "./log",
        const char* suffix = ".log");

    /**
     * 异步写日志公有方法，调用私有方法asyncWrite
     */
    static void FlushLog()
    {
        Log::GetInstance()->AsyncWrite();
    }

    /**
     * 将输出内容按照标准格式整理
     */
    void WriteLog(int level, const char* format, ...);

private:
    Log()
        : m_line_count(0)
        , m_today(0)
        , m_fp(nullptr)
        , m_deque(nullptr)
        , m_write_thread(nullptr)
    {
    }
    ~Log();

    void AsyncWrite();

private:
    const int LOG_NAME_LEN = 256; // 日志文件最长文件名
    const int MAX_LOG_LINES = 50000; // 日志文件内的最长日志条数

    const char* m_path; // 路径名
    const char* m_suffix; // 后缀名
    int m_line_count; // 日志行数记录
    int m_today; // 按当天日期区分文件
    FILE* m_fp; // 打开log的文件指针
    Buffer m_buff; // 输出的内容
    std::unique_ptr<BlockQueue<std::string>> m_deque; // 阻塞队列
    std::unique_ptr<std::thread> m_write_thread; // 写线程
    std::mutex m_mtx; // 同步日志必需的互斥量
};

// 四个宏定义，主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) Log::GetInstance()->WriteLog(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::GetInstance()->WriteLog(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::GetInstance()->WriteLog(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::GetInstance()->WriteLog(3, format, ##__VA_ARGS__)

#endif // !_LOGGER_H_