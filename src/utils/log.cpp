#include "log.h"
#include <cassert>
#include <memory>

Log::~Log()
{
    if (m_write_thread && m_write_thread->joinable()) {
        while (!m_deque->empty()) // 清空阻塞队列中的全部任务
        {
            m_deque->flush();
        }
        m_deque->Close();
        m_write_thread->join(); // 等待当前线程完成手中的任务
    }
    if (m_fp) // 冲洗文件缓冲区，关闭文件描述符
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        fflush(m_fp);
        fclose(m_fp);
    }
}

void Log::Init(int max_queue_capacity, const char* path, const char* suffix)
{
    assert(max_queue_capacity > 0);
    if (!m_deque) {
        auto new_deque = std::make_unique<BlockQueue<std::string>>(max_queue_capacity);
        m_deque = std::move(new_deque);
        auto new_thread = std::make_unique<std::thread>(FlushLog);
        m_write_thread = std::move(new_thread);
    }

    m_line_count = 0;
    time_t timer = time(nullptr);
    struct tm* sysTime = localtime(&timer);
    struct tm t = *sysTime;
    m_path = path;
    m_suffix = suffix;
    char filename[LOG_NAME_LEN];
    snprintf(filename, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
        m_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, m_suffix);
    m_today = t.tm_mday;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_buff.Clear();
        if (m_fp) {
            fflush(m_fp);
            fclose(m_fp);
        }
        m_fp = fopen(filename, "a");
        if (m_fp == nullptr) {
            mkdir(m_path, 0777); // 先生成目录文件（最大权限）
            m_fp = fopen(filename, "a");
        }
        assert(m_fp != nullptr);
    }
}

void Log::WriteLog(int level, const char* format, ...)
{
    struct timeval now = { 0, 0 };
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm* sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    if (m_today != t.tm_mday || (m_line_count && (m_line_count % MAX_LOG_LINES == 0))) {
        // 生成最新的日志文件名
        char newFile[LOG_NAME_LEN];
        char tail[36] = { 0 };
        snprintf(tail, 35, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        if (m_today != t.tm_mday) // 时间不匹配，则替换为最新的日志文件名
        {
            snprintf(newFile, LOG_NAME_LEN - 1, "%s/%s%s", m_path, tail, m_suffix);
            m_today = t.tm_mday;
            m_line_count = 0;
        } else // 长度超过日志最长行数，则生成xxx-1、xxx-2文件
        {
            snprintf(newFile, LOG_NAME_LEN - 1, "%s/%s-%d%s", m_path, tail, (m_line_count / MAX_LOG_LINES), m_suffix);
        }

        if (m_fp) {
            std::lock_guard<std::mutex> lock(m_mtx);
            fflush(m_fp);
            fclose(m_fp);
        }
        m_fp = fopen(newFile, "a");
        // assert(m_fp != nullptr); // TODO:DEBUG
    }

    // 在buffer内生成一条对应的日志信息
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_line_count++;
        int n = snprintf(m_buff.GetWritePtr(), 128, "%04d-%02d-%02d %02d:%02d:%02d.%06ld ", // 添加年月日时分秒微秒———"2022-12-29 19:08:23.406500"
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        m_buff.AddWritePos(n);
        switch (level) { // 添加日志等级———"2022-12-29 19:08:23.406539 [debug]: "
        case 0:
            m_buff.Append("[debug]: ");
            break;
        case 1:
            m_buff.Append("[info] : ");
            break;
        case 2:
            m_buff.Append("[warn] : ");
            break;
        case 3:
            m_buff.Append("[error]: ");
            break;
        default:
            m_buff.Append("[info] : ");
            break;
        }

        va_start(vaList, format);
        int m = vsnprintf(m_buff.GetWritePtr(), m_buff.WriteableBytes(), format, vaList); // 添加使用日志时的格式化输入———"2022-12-29 19:08:23.535531 [debug]: Test 222222222 8 ============= "
        va_end(vaList);
        m_buff.AddWritePos(m);
        // 添加换行符与字符串结尾
        m_buff.Append("\n\0");
    }

    if (m_deque && !m_deque->full()) // 异步方式（加入阻塞队列中，等待写线程读取日志信息）
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        std::string s = std::string(m_buff.GetReadPtr(), m_buff.GetWritePtr());
        m_deque->push_back(s);
    }

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_buff.Clear(); // 清理buffer缓冲区
    }
}

void Log::AsyncWrite()
{
    std::string str = "";
    while (m_deque->pop(str)) {
        std::lock_guard<std::mutex> lock(m_mtx);
        fputs(str.c_str(), m_fp);
    }
}