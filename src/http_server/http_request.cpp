#include "http_request.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    { "/register.html", 0 },
    { "/login.html", 1 },
};

static int ConvertHex(char ch)
{
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

void HttpRequest::Reset()
{
    m_method = m_body = m_path = m_version = "";
    m_state = CHECK_STATE_REQUESTLINE;
    m_header.clear();
    m_post.clear();
}

std::string HttpRequest::GetPost(const std::string& key)
{
    assert(key != "");
    if (m_post.count(key) == 1) {
        return m_post.at(key);
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const
{
    if (m_header.count("Connection") == 1) {
        return m_header.at("Connection") == "keep-alive" && m_version == "1.1";
    }
    return false;
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
        const std::string line(buff.GetReadPtr(), lineEnd); // 根据分割符提取出一个完整的行
        switch (m_state) { // 根据状态机状态跳转
        case CHECK_STATE_REQUESTLINE:
            if (!ParseRequestLine(line)) { // 解析请求行，解析请求行失败代表请求格式错误
                return false;
            }
            m_state = CHECK_STATE_HEADER; // 解析成功，转移状态
            break;
        case CHECK_STATE_HEADER:
            if (!ParseHeader(line)) { // 解析一行请求头，返回false代表本行不是请求头，即请求头全部解析完成
                if (buff.ReadableBytes() <= 2) { // 若只剩下\r\n，说明没有请求体
                    m_state = CHECK_STATE_FINISH; // 解析结束
                } else {
                    m_state = CHECK_STATE_BODY; // 转移状态至解析请求体
                }
            }
            break;
        case CHECK_STATE_BODY:
            m_body = line;
            if (!ParseBody(line)) { // 解析请求体，解析请求体失败代表请求体格式错误
                return false;
            }
            m_state = CHECK_STATE_FINISH; // 解析成功，转移状态
            break;
        default:
            break;
        }
        if (lineEnd == buff.GetWritePtr()) { // 没找到分隔符
            if (m_method == "POST" && m_state == CHECK_STATE_FINISH) { // POST请求体最后直接结束，没有分隔符
                buff.SetReadPos(lineEnd); // readpos +=
            }
            break;
        }
        buff.SetReadPos(lineEnd + 2); // readpos +=
    }
    return true;
}

bool HttpRequest::ParseRequestLine(const std::string& line)
{
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        m_method = subMatch[1];
        m_path = subMatch[2];
        m_version = subMatch[3];

        /* 若path存在，补全其路径 */
        if (m_path == "/") { // 根目录
            m_path = "/index.html";
        } else {
            for (auto& item : DEFAULT_HTML) {
                if (item == m_path) {
                    m_path += ".html";
                    break;
                }
            }
        }

        return true;
    }
    return false;
}

bool HttpRequest::ParseHeader(const std::string& line)
{
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        m_header[subMatch[1]] = subMatch[2]; // 设置请求头的属性集
        return true;
    }
    return false;
}

bool HttpRequest::ParseBody(const std::string& line)
{
    if (m_method == "GET") // GET方法不支持body传参，直接忽略
        return true;

    if (m_method == "POST" && m_header["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded();
        if (DEFAULT_HTML_TAG.count(m_path) == 1) {
            int tag = DEFAULT_HTML_TAG.at(m_path);
            if (tag == 0 || tag == 1) {
                bool is_login = (tag == 1); // 实现登录功能
                if (UserVerify(m_post["username"], m_post["password"], is_login)) {
                    m_path = "/welcome.html";
                } else {
                    m_path = "/error.html";
                }
                return true;
            }
        }
    }

    return false;
}

// POST请求体示例：action=user_login&username=%E5%8F%91&password=+%E5%8F%91&rememberme=1
void HttpRequest::ParseFromUrlencoded()
{
    if (m_body.size() == 0) {
        return;
    }

    std::string key, value;
    int num = 0;
    int n = m_body.size();
    int i = 0, j = 0;

    for (; i < n; i++) {
        char ch = m_body[i];
        switch (ch) {
        case '=':
            key = m_body.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            m_body[i] = ' ';
            break;
        case '%':
            num = ConvertHex(m_body[i + 1]) * 16 + ConvertHex(m_body[i + 2]);
            m_body[i + 2] = num % 10 + '0';
            m_body[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = m_body.substr(j, i - j);
            j = i + 1;
            m_post[key] = value;
            // LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if (m_post.count(key) == 0 && j < i) {
        value = m_body.substr(j, i - j);
        m_post[key] = value;
    }
}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin)
{
    if (name.empty() || pwd.empty()) {
        return false;
    }
    //    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql = SqlConnector::GetInstance().GetConnection();
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;

    if (!isLogin) {
        flag = true;
    }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    //    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        //        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        /* 登录行为 */
        if (isLogin) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                //                LOG_DEBUG("pwd error!");
            }
        } else {
            flag = false;
            //            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag) {
        //        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        //        LOG_DEBUG( "%s", order);
        if (mysql_query(sql, order)) {
            //            LOG_DEBUG( "Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnector::GetInstance().FreeConnection(sql);
    //    LOG_DEBUG( "UserVerify success!!");
    return flag;
}
