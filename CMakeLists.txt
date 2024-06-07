cmake_minimum_required (VERSION 3.0)

project (webServer)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)		# 让CMake生成compile_commands.json文件
set(CMAKE_BUILD_TYPE "Debug")				# 设置编译时使用 -g 参数生成可供调试的程序，不设置则生成的main.exe无法调试

include_directories(${PROJECT_BINARY_DIR}/../src/buffer ${PROJECT_BINARY_DIR}/../src/http_server ${PROJECT_BINARY_DIR}/../src/utils)

aux_source_directory(${PROJECT_BINARY_DIR}/../src/buffer BUFFER)
aux_source_directory(${PROJECT_BINARY_DIR}/../src/http_server HTTP_SERVER)
aux_source_directory(${PROJECT_BINARY_DIR}/../src/utils UTILS)
# aux_source_directory(${PROJECT_BINARY_DIR}/../src MAIN)

# find_library(LIB webserver ${PROJECT_BINARY_DIR}/../lib)

set(CMAKE_CXX_FLAGS "-I/usr/include/mysql")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../)
add_executable(main ${BUFFER} ${HTTP_SERVER} ${UTILS} ${PROJECT_BINARY_DIR}/../src/main.cpp)
target_link_libraries(main -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lzstd -lssl -lcrypto -lresolv -lm)

# target_link_libraries(main ${LIB})