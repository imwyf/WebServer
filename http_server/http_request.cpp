#include "http_request.h"
#include <cassert>
#include <string>

// TODO:修改
std::string HttpRequest::GetPost(const std::string& key)
{
    assert(key != "");
    if (m_post.contains(key)) {
        return m_post.at(key);
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const
{
    if (m_header.count("Connection") == 1) {
        return m_header.find("Connection")->second == "keep-alive" && m_version == "1.1";
    }
    return false;
}

void HttpRequest::Init()
{
    m_method = m_body = m_path = m_version = "";
    m_state = CHECK_STATE_REQUESTLINE;
    m_header.clear();
    m_post.clear();
}

/**
 * 解析请求的主方法:从读缓冲中读入内容，以\r\n作为行分割，分别解析请求的3个部分
 */
bool HttpRequest::Parse(Buffer& buff)
{
    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() <= 0) {
        return false;
    }
    while (buff.ReadableBytes() && m_state != CHECK_STATE_FINISH) { // 状态机，循环解析
        char* lineEnd = std::search(buff.GetReadPtr(), buff.GetWritePtr(), CRLF, CRLF + 2); // 在可读数据中找到分隔符\r\n
        std::string line(buff.GetReadPtr(), lineEnd); // 根据分割符提取出一个完整的行
        switch (m_state) { // 根据状态机状态跳转
        case CHECK_STATE_REQUESTLINE:
            if (!ParseRequestLine(line)) { // 解析请求头
                return false;
            }
            ParsePath(); // 解析要请求的资源
            break;
        case CHECK_STATE_HEADER:
            ParseHeader(line); // 解析请求头
            if (buff.ReadableBytes() <= 2) { // 可能没有请求体
                m_state = CHECK_STATE_FINISH;
            }
            break;
        case CHECK_STATE_CONTENT:
            ParseBody(line); // 解析请求体
            break;
        default:
            break;
        }
        if (lineEnd == buff.GetWritePtr()) { //
            if (m_method == "POST" && m_state == CHECK_STATE_FINISH) {
                buff.SetReadPos(lineEnd); // readpos += xxx
            }
            break;
        }
        buff.SetReadPos(lineEnd + 2); // readpos += xxx
    }
    return true;
}