#include "http_response.h"
#include "http_connector.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html", "text/html" },
    { ".xml", "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt", "text/plain" },
    { ".rtf", "application/rtf" },
    { ".pdf", "application/pdf" },
    { ".word", "application/nsword" },
    { ".png", "image/png" },
    { ".gif", "image/gif" },
    { ".jpg", "image/jpeg" },
    { ".jpeg", "image/jpeg" },
    { ".au", "audio/basic" },
    { ".mpeg", "video/mpeg" },
    { ".mpg", "video/mpeg" },
    { ".avi", "video/x-msvideo" },
    { ".gz", "application/x-gzip" },
    { ".tar", "application/x-tar" },
    { ".css", "text/css " },
    { ".js", "text/javascript " },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

void HttpResponse::Reset(const std::string& src_dir, std::string path, bool is_keepalive, int code)
{
    assert(!src_dir.empty());
    if (m_file) {
        UnmapFile();
    }
    m_code = code;
    m_is_keepalive = is_keepalive;
    m_path = path;
    m_src_dir = src_dir;
    m_file = nullptr;
    m_file_stat = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff)
{
    if (stat((m_src_dir + m_path).data(), &m_file_stat) < 0 || S_ISDIR(m_file_stat.st_mode)) { // 判断请求的资源文件是否存在
        m_code = 404;
    } else if (!(m_file_stat.st_mode & S_IROTH)) {
        m_code = 403;
    } else if (m_code == -1) {
        m_code = 200;
    }
    ErrorHtml();
    AddStateLine(buff); // 添加响应行
    AddHeader(buff); // 添加响应头
    AddContent(buff); // 添加响应体
}

void HttpResponse::ErrorHtml()
{
    if (CODE_PATH.count(m_code) == 1) {
        m_path = CODE_PATH.at(m_code);
        stat((m_src_dir + m_path).data(), &m_file_stat);
    }
}

void HttpResponse::AddStateLine(Buffer& buff)
{
    std::string status;
    if (CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.at(m_code);
    } else {
        m_code = 400;
        status = CODE_STATUS.at(400);
    }
    buff.Append("HTTP/1.1 " + std::to_string(m_code) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer& buff)
{
    buff.Append("Connection: ");
    if (m_is_keepalive) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType() + "\r\n");
}

void HttpResponse::AddContent(Buffer& buff)
{
    int src_fd = open((m_src_dir + m_path).data(), O_RDONLY);
    if (src_fd < 0) {
        ErrorContent(buff, "File Not Found!");
        return;
    }

    /* 将文件映射到内存提高文件的访问速度
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    // mmap 将一个文件或者其它对象映射进内存。文件被映射到多个页上，如果文件的大小不是所有页的大小之和，最后一个页不被使用的空间将会清零。
    // munmap 执行相反的操作，删除特定地址区域的对象映射。
    // LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mm_ret = (int*)mmap(nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
    if (*mm_ret == -1) {
        ErrorContent(buff, "File Not Found!");
        return;
    }
    m_file = (char*)mm_ret;
    close(src_fd);
    buff.Append("Content-length: " + std::to_string(m_file_stat.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile()
{
    if (m_file) {
        munmap(m_file, m_file_stat.st_size);
        m_file = nullptr;
    }
}

std::string HttpResponse::GetFileType()
{
    std::string::size_type idx = m_path.find_last_of('.');
    if (idx == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = m_path.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.at(suffix);
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message) const
{
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.at(m_code);
    } else {
        status = "Bad Request";
    }
    body += std::to_string(m_code) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}