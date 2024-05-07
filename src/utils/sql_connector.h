#include <condition_variable>
#include <mutex>
#include <mysql/mysql.h>
#include <queue>

/**
 * sqlconnector连接池
 */
class SqlConnector {
public:
    static SqlConnector& GetInstance() // 懒汉单例模式
    {
        static SqlConnector m_instance;
        return m_instance;
    }
    ~SqlConnector() { ClosePool(); }

    /**
     * 初始化连接池属性
     */
    void InitPool(const char* host, int port,
        const char* user, const char* pwd,
        const char* db_name, int conn_size = 8);
    /**
     * 从池中获取连接
     */
    MYSQL* GetConnection();
    /**
     * 释放连接，归还池中
     */
    void FreeConnection(MYSQL* conn);
    /**
     * 关闭连接池
     */
    void ClosePool();

    int GetPoolSize() { return m_max_conn_size; }

private:
    /* 下面的声明是为了实现单例模式 */
    SqlConnector() = default;
    SqlConnector(const SqlConnector& obj) = delete;
    SqlConnector& operator=(const SqlConnector& rhs) = delete;

    /* 下面是连接池的属性 */
    static int m_max_conn_size; // 连接队列最大大小
    std::queue<MYSQL*> m_conn_queue; // 连接队列
    std::mutex m_mutex; // 互斥锁
    std::condition_variable m_condition; // 条件变量
};