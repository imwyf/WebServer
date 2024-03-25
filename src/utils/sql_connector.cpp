#include "sql_connector.h"
#include <cassert>
#include <mutex>

void SqlConnector::InitPool(const char* host, int port,
    const char* user, const char* pwd,
    const char* db_name, int conn_size)
{
    assert(conn_size > 0);

    for (int i = 0; i < conn_size; i++) { // 预先在队列中创建连接
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (sql == nullptr) {
            //            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd, db_name, port, nullptr, 0);
        if (sql == nullptr) {
            //            LOG_ERROR("MySql connect error!");
        }
        m_conn_queue.push(sql);
    }

    m_max_conn_size = conn_size;
}

MYSQL* SqlConnector::GetConnection()
{
    MYSQL* sql = nullptr;
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        if (m_conn_queue.empty()) { // 队列为空，返回nullptr
            //        LOG_WARN("SqlConnPool busy!");
        } else {
            m_condition.wait(locker,
                [this] { return !this->m_conn_queue.empty(); }); // 利用条件变量等待队列不为空
            sql = m_conn_queue.front();
            m_conn_queue.pop();
        }
    }
    return sql;
}

void SqlConnector::FreeConnection(MYSQL* sql)
{
    assert(sql);
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_conn_queue.push(sql);
    }
}

void SqlConnector::ClosePool()
{
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        while (!m_conn_queue.empty()) {
            auto item = m_conn_queue.front();
            m_conn_queue.pop();
            mysql_close(item);
        }
        mysql_library_end();
    }
}