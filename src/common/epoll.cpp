#include "../../include/epoll.h"
#include "spdlog/spdlog.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <stdexcept>
#include <iostream>

//Epoll系统用于服务端与多个客户端连接时的事件通知系统
//其工作流程为：创建epoll实例 -> 添加监听描述符 -> 等待事件发生 -> 处理事件

Epoll::Epoll() : epoll_fd_(-1), is_created_(false) {}

Epoll::~Epoll() {
    if (is_created_) {
        close();
    }
}

bool Epoll::create() {
    if (is_created_) {
        std::cerr << "Epoll already created" << std::endl;
        return false;
    }

    //创建epoll文件描述符
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "epoll_create error" << std::endl;
        return false;
    }

    is_created_ = true;
    return true;
}

//添加监听
bool Epoll::add(int fd, uint32_t events) {
    if (!is_created_) {
        std::cerr << "Epoll not created" << std::endl;
        return false;
    }
    if (fd < 0) {
        std::cerr << "Invalid fd" << std::endl;
        return false;
    }

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        std::cerr << "epoll_ctl error" << std::endl;
        return false;
    }

    return true;
}

bool Epoll::modify(int fd, uint32_t events) {
    if (!is_created_) {
        std::cerr << "Epoll not created" << std::endl;
        return false;
    }

    if (fd < 0) {
        std::cerr << "Invalid fd" << std::endl;
        return false;
    }

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        std::cerr << "epoll_ctl error" << std::endl;
        return false;
    }

    return true;
}

bool Epoll::remove(int fd) {
    if (!is_created_) {
        std::cerr << "Epoll not created" << std::endl;
        return false;
    }

    if (fd < 0) {
        std::cerr << "Invalid fd" << std::endl;
        return false;
    }

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        std::cerr << "epoll_ctl error" << std::endl;
        return false;
    }

    return true;
}

std::vector<struct epoll_event> Epoll::wait(int timeout) {
    std::vector<struct epoll_event> events;
    if (!is_created_) {
        std::cerr << "Epoll not created" << std::endl;
        return events;
    }

    const int MAX_EVENTS = 1024;
    struct epoll_event ev_list[MAX_EVENTS];

    int nfds = epoll_wait(epoll_fd_, ev_list, MAX_EVENTS, timeout);

    if (nfds < 0) {
        if (errno == EINTR) {
            return events;
        }
        std::cerr << "epoll_wait error" << std::endl;
        return events;
    }

    events.reserve(nfds);
    for (int i = 0; i < nfds; i++) {
        events.emplace_back(ev_list[i]);
    }
    return events;
}

void Epoll::close() {
    if (!is_created_) {
        return;
    }

    if (epoll_fd_ > 0) {
        ::close (epoll_fd_);
        epoll_fd_ = -1;
    }
    is_created_ = false;
}
