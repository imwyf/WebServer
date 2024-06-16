#ifndef _HTTP_RESPONSE_H
#define _HTTP_RESPONSE_H

#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

#include "../buffer/buffer.h"

/**
 * 用于生成http应答报文的类，不包含写缓冲，目的是通过解析结果生成应答报文，并写入写缓冲
 */
class HttpResponse {
public:
    HttpResponse()
        : m_code(-1)
        , m_path("")
        , m_src_dir("")
        , m_is_keepalive(false)
        , m_file(nullptr)
        , m_file_stat({ 0 })
    {
    }
    ~HttpResponse() { UnmapFile(); }
    /**
     * 根据request解析的结果，将参数传递至response，并重置HttpResponse中应写入的内容
     */
    void Init(const std::string& src_dir, std::string path, bool is_Keepalive, int code);

    /**
     * 主入口函数，根据解析结果生成应答报文并写入buff
     */
    void MakeResponse(Buffer& buff);

    void UnmapFile();
    /**
     * 获取资源文件
     */
    char* GetFile() { return m_file; }
    size_t FileLen() const { return m_file_stat.st_size; }
    /**
     * 向buff直接写入出错信息
     */
    void ErrorContent(Buffer& buff, std::string message) const;
    int GetCode() const { return m_code; }

private:
    /* 下面三个函数分别用来填充应答报文的三个部分 */

    void AddStateLine(Buffer& buff);
    void AddHeader(Buffer& buff);
    /**
     * 与添加状态行和头部不同，添加应答体是将转文件映射到内存中，并设置m_file指针，等待后续writev直接写出
     */
    void AddContent(Buffer& buff);
    /**
     * 如果有的话，设置path指向错误相应的html界面
     */
    void ErrorHtml();
    /**
     * 获取资源文件类型
     */
    std::string GetFileType();

    int m_code; // 状态码
    bool m_is_keepalive; // 是否长连接

    std::string m_path; // 应答报文指向的资源路径
    std::string m_src_dir; // 根目录

    char* m_file; // 实际的资源文件在内存中的位置
    struct stat m_file_stat; // 资源文件状态

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE; // 文件类型与对应的应答头的映射
    static const std::unordered_map<int, std::string> CODE_STATUS; // 状态码
    static const std::unordered_map<int, std::string> CODE_PATH; // 状态码对应的错误网页文件的映射
};

#endif // _HTTP_RESPONSE_H