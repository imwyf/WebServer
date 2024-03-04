#include "http_conn.h"
#include <netinet/in.h>
int main()
{
    HttpConn http_conn;
    http_conn.InitConn(5, sockaddr_in());
}