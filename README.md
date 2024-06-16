# WebServer
用C++实现的高性能WEB服务器，经过webbenchh压力测试可以实现上万的QPS

- 环境配置 environment
```
ubuntu 22.04
c++17
libmysqlclient-dev
mysql 8.0
```

mysql root password:111111

```sql
create database webserver;
use webserver;

create table user(
    username varchar(50),
    password varchar(50)
    );
```

- 编译 compile
```
mkdir build
cd build
cmake ..
make 
```

- 运行 run
```
./main
```
- 压测
```
./webbench-1.5/webbench -c 10000 -t 5 http://localhost:8080/
./webbench-1.5/webbench -c 1000 -t 5 http://localhost:8080/
./webbench-1.5/webbench -c 100 -t 5 http://localhost:8080/
```

## 致谢
Linux高性能服务器编程，游双著.
