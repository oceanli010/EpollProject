cmake_minimum_required(VERSION 3.15)
project(NetworkProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译器警告标志
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O2")
endif()

# 包含目录
include_directories(include)

# 正确的方式：查找线程库
find_package(Threads REQUIRED)

# 公共源文件
set(COMMON_SOURCES
        src/common/epoll.cpp
        src/common/socket.cpp
)

# 服务器可执行文件
set(SERVER_SOURCES
        src/server/server.cpp
        ${COMMON_SOURCES}
)

# 客户端可执行文件
set(CLIENT_SOURCES
        src/client/client.cpp
        src/common/socket.cpp
)

# 创建服务器可执行文件
add_executable(server ${SERVER_SOURCES})
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# 正确链接线程库
target_link_libraries(server ${CMAKE_THREAD_LIBS_INIT})

# 创建客户端可执行文件
add_executable(client ${CLIENT_SOURCES})
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(client ${CMAKE_THREAD_LIBS_INIT})

cmake_minimum_required(VERSION 3.10)
project(EpollProject)
