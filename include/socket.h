#pragma once

#include <string>
#include <vector>

class Socket {
public:
    Socket();
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    bool createSocket();
    bool bindSocket(int port);
    bool listenSocket(int backlog);
    Socket acceptSocket();
    bool connect(const std::string& ip, int port);

    ssize_t send(const std::vector<char>& data);
    ssize_t send(const std::string& data);
    ssize_t recv(std::vector<char>& buffer, size_t size);

    bool setNonBlocking(bool nonblock = true);

    void close();

    int getFd() const{ return fd_; }
    bool isValid() const { return fd_ != -1; }
    std::string getPeerAddress() const;
    int getPeerPort() const;

private:
    int fd_;
    bool is_non_blocking_;
};