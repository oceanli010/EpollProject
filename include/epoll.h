#pragma once

#include <vector>
#include <cstdint>

#include <sys/epoll.h>

class Epoll {
public:
    Epoll();
    ~Epoll();

    bool create();

    bool add(int fd, uint32_t events);

    bool modify(int fd, uint32_t events);

    bool remove(int fd);

    std::vector<struct epoll_event> wait(int timeout = -1);

    int getFd() const { return epoll_fd_; }

    void close();

private:
    int epoll_fd_;
    bool is_created_;
};

namespace EpollEvents {
    constexpr uint32_t IN = EPOLLIN;
    constexpr uint32_t OUT = EPOLLOUT;
    constexpr uint32_t ERR = EPOLLERR;
    constexpr uint32_t HUP = EPOLLHUP;
    constexpr uint32_t ET = EPOLLET;
    constexpr uint32_t ONESHOT = EPOLLONESHOT;
}