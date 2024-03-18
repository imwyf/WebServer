#ifndef _HTTP_REQUEST_
#define _HTTP_REQUEST_

#include "../buffer/buffer.h"
#include "../mysql/mysql.h"
#include <cerrno>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

/* 注：HTTP报文的格式参考
----------------------------------1.发送--------------------------------------------
            | POST http://www.baidu.com HTTP/1.1                         \r\n       (请求行)
            | Host: api.efxnow.com                                       \r\n       (一条请求头)
            | Content-Type: application/x-www-form-urlencoded            \r\n       (一条请求头)
            | Content-Length: length                                     \r\n       (一条请求头)
            |                                                            \r\n       (空行)
            | UserID=string&PWD=string&OrderConfirmation=string          \r\n       (请求体)
----------------------------------2.应答--------------------------------------------
            | HTTP/1.1 200 OK
            | Content-Type: text/xml; charset=utf-8
            | Content-Length: length
            |
            | <? xml version = "1.0" encoding = "utf-8" ?>
            | < objPlaceOrderResponse xmlns = "https://api.efxnow.com/webservices2.3" >
            | < Success >boolean</ Success >
            | < ErrorDescription >string</ ErrorDescription >
            | < ErrorNumber >int</ ErrorNumber >
            | < CustomerOrderReference >long</ CustomerOrderReference >
            | < OrderConfirmation >string</ OrderConfirmation >
            | < CustomerDealRef >string</ CustomerDealRef >
            | </ objPlaceOrderResponse >
 */

/**
 * 用于处理http请求报文的类
 */
class HttpRequest {
public:
    enum HttpCheckState {
        CHECK_STATE_REQUESTLINE, // 正在分析请求行
        CHECK_STATE_HEADER, // 正在分析头部字段
        CHECK_STATE_BODY, // 正在分析请求体
        CHECK_STATE_FINISH // 分析完毕
    };
    enum HttpMethod {
        GET,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };
    enum HttpCode {
        NO_REQUEST, // 请求不完整，需要继续读取客户数据（服务器内部使用）
        GET_REQUEST, // 获得了一个完整的客户请求（服务器内部使用）
        FILE_REQUEST, // 文件请求到了（服务器内部使用）
        OK = 200, // 访问成功
        BAD_REQUEST = 400, // 客户请求有语法错误
        FORBIDDEN_REQUEST = 403, // 客户没有权限访问该资源
        NOT_FOUND = 404, // 客户访问的资源没有找到
        CLOSED_CONNECTION = 499, // 连接已关闭
        INTERNAL_SERVER_ERROR = 500, // 服务器内部错误
    };
    enum HttpVersion {
        HTTP_1_0 = 10,
        HTTP_1_1 = 11
    };
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    /**
     * 从buff中读取每一行来解析的主方法入口
     */
    bool Parse(Buffer& buff);

    std::string GetPath() const { return m_path; }
    std::string GetMethod() const { return m_method; }
    std::string GetVersion() const { return m_version; }
    std::string GetPost(const std::string& key);
    bool IsKeepAlive() const;

private:
    /* 下面的3个方法被解析请求的主方法Parse调用以分别解析请求的3个部分 */

    /**
     * 解析请求行的入口方法，返回true代表解析成功，返回false代表本行不是请求行的正确格式
     */
    bool ParseRequestLine(const std::string& line);
    /**
     * 解析一行请求头的入口方法，返回true代表解析成功，返回false代表本行不是请求头的正确格式
     */
    bool ParseHeader(const std::string& line);
    /**
     *  解析请求体的入口方法，返回true代表解析成功，返回false代表本行不是请求体的正确格式
     */
    bool ParseBody(const std::string& line);

    void ParseFromUrlencoded();

    HttpCheckState m_state;
    std::string m_method, m_version, m_path; // 解析后请求行的三个属性之一
    std::unordered_map<std::string, std::string> m_header; // 解析后本请求的头部属性
    std::string m_body; // 保存的请求体
    std::unordered_map<std::string, std::string> m_post; // 解析后本请求的请求体中post上来的属性

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConvertHex(char ch);
};

#endif // _HTTP_REQUEST_