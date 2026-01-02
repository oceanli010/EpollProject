#include "../../include/socket.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstring>
#include <stdexcept>
#include <iostream>

//初始化Socket类，标记为未连接状态
Socket::Socket() : fd_(-1), is_non_blocking_(false) {}

//如果连接仍然存活，需要关闭连接后才能回收资源
Socket::~Socket() {
    if (fd_ != -1) {
        close();
    }
}

//移动构造，需要将构造的新连接设置为未连接状态
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_), is_non_blocking_(other.is_non_blocking_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (fd_ != -1) {
            close();
        }
        fd_ = other.fd_;
        is_non_blocking_ = other.is_non_blocking_;
        other.fd_ = -1;
    }
    return *this;
}

//创建Socket套接字
bool Socket::createSocket() {
    //参数说明：domain - 协议簇(AF_INIT代表IPv4协议)
    //type - 指定socket类型(SOCK_STREAM代表流式套接字)
    //protocol - 指定协议类型，为0时自动匹配type支持的协议
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    int opt = 1;    //启用 SO_REUSEADDR 字段的标志位
    //setsockopt：设置套接字选项
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close();
        return false;
    }

    return true;
}

//将套接字绑定至端口号
/*
 * 说明：结构体sockaddr_in的构成如下：
 * struct sockaddr_in{
 *      sa_family_t     sin_family
 *      in_port_t       sin_port
 *      struct in_addr  sin_addr
 * };
 *
 * struct in_addr{
 *      uint32_t     s_addr
 * }
 */
bool Socket::bindSocket(int port) {
    if (fd_ == -1) {
        std::cerr << "Socket bind failed" << std::endl;
        return false;
    }

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));  //声明后初始化为0，避免读取到垃圾值

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port); //htons将端口号转为大端序（高位在前）

    if (::bind(fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return false;
    }

    return true;
}

//监听消息
//backlog 最大排队数
bool Socket::listenSocket(int backlog) {
    if (fd_ == -1) {
        std::cerr << "Socket listen failed" << std::endl;
        return false;
    }

    if (::listen(fd_, backlog) < 0) {
        perror("listen failed");
        return false;
    }

    return true;
}

//接受连接请求
Socket Socket::acceptSocket() {
    if (fd_ == -1) {
        throw std::runtime_error("Socket accept failed");
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    //以阻塞方式等待客户端连接
    int client_fd = ::accept(fd_, (struct sockaddr*)&client_addr, &client_addr_len);

    if (client_fd < 0) {
        //在非阻塞模式下，返回空的socket，表示暂时没有新的连接
        //在后续检查这里的socket是否有效
        if (is_non_blocking_ && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return Socket();
        }
        perror("accept failed");
        return Socket();
    }

    Socket client_socket;
    client_socket.fd_ = client_fd;
    client_socket.is_non_blocking_ = is_non_blocking_;

    return client_socket;
}

//建立连接
bool Socket::connect(const std::string& ip, int port) {
    if (fd_ == -1) {
        std::cerr << "Socket connect failed" << std::endl;
        return false;
    }

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    //将文本地址转为二进制地址，并复制到server_addr结构体
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        return false;
    }

    if (::connect(fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        return false;
    }

    return true;
}

//发送和接收数据
ssize_t Socket::send(const std::vector<char>& data) {
    if (fd_ == -1) {
        std::cerr << "Socket send failed" << std::endl;
        return -1;
    }

    return ::send(fd_, data.data(), data.size(), 0);
}

ssize_t Socket::send(const std::string& data) {
    return send(std::vector<char>(data.begin(), data.end()));
}

ssize_t Socket::recv(std::vector<char>& buffer, size_t size) {
    if (fd_ == -1) {
        std::cerr << "Socket recv failed" << std::endl;
        return -1;
    }
    if (size == 0) {
        return 0;
    }

    buffer.resize(size);
    ssize_t bytes_recv = ::recv(fd_, buffer.data(), size, 0);

    if (bytes_recv < 0) {
        buffer.clear();
        if (!is_non_blocking_ || (errno != EAGAIN && errno != EWOULDBLOCK )) {
            perror("recv failed");
        }
        return -1;
    } else {
        buffer.resize(bytes_recv);
    }

    return bytes_recv;
}

bool Socket::setNonBlocking(bool nonblock) {
    if (fd_ == -1) {
        std::cerr << "Socket setNonBlocking failed" << std::endl;
        return false;
    }

    int flags = fcntl(fd_, F_GETFL, 0); //获取当前标志
    if (flags < 0) {
        perror("fcntl failed");
        return false;
    }

    if (nonblock) {
        flags |= O_NONBLOCK;    //添加非阻塞标志
    } else {
        flags &= ~O_NONBLOCK;   //清除非阻塞标志
    }

    if (fcntl(fd_, F_SETFL, flags) < 0) {
        perror("fcntl failed");
        return false;
    }

    is_non_blocking_ = nonblock;
    return true;
}

void Socket::close() {
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
        is_non_blocking_ = false;
    }
}

std::string Socket::getPeerAddress() const {
    if (fd_ == -1) { return ""; }

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getpeername(fd_, (struct sockaddr*)&addr, &len) < 0) {
        return "";
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);

    return std::string(ip_str);
}

int Socket::getPeerPort() const {
    if (fd_ == -1) { return -1; }

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getpeername(fd_, (struct sockaddr*)&addr, &len) < 0) {
        return -1;
    }

    return ntohs(addr.sin_port);
}
