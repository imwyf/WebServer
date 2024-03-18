#include "http_response.h"

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

void HttpResponse::Init(const std::string& srcDir, std::string path, bool isKeepAlive, int code)
{
    assert(!srcDir.empty());
    if (mmFile_) {
        UnmapFile();
    }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}
