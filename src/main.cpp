#include "http_server/http_server.h"

/*
 * WebServer服务的启动入口函数，应该被编译名为main的可执行文件
 * 可在此更改监听端口，连接池、线程池参数及其他参数
 */
int main(int argc, char* argv[])
{
    HttpServer server(8080, 60000, true, 8, true, 3306, "root", "111111", "webserver", 8);
    server.Start();
}
