#include "../../include/socket.h"
#include "../../include/epoll.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"  // 彩色控制台
#include "spdlog/sinks/basic_file_sink.h"     // 文件日志

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>
#include <memory>

#include <signal.h>
#include <unistd.h>

class EpollServer {
private:
    Socket server_socket_;
    Epoll epoll_;
    volatile bool running_;
    std::unordered_map<int, std::unique_ptr<Socket>> clients_;
    std::shared_ptr<spdlog::logger> logger_;

    static const int MAX_EVENTS = 1024;
    static const int PORT = 8080;
    static const int BACKLOG = 128;

public:
    EpollServer() : running_(false) {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/server.log", true);

            std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
            logger_ = std::make_shared<spdlog::logger>("server_logger", sinks.begin(), sinks.end());
            logger_->set_level(spdlog::level::info);
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

            spdlog::register_logger(logger_);
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr <<"spdlog init failed" << ex.what() << std::endl;
        }
    }

    ~EpollServer() {
        stop();
    }

    bool start() {
        if (!server_socket_.createSocket()) {
           logger_->error("Failed to create server socket");
            return false;
        }

        if (!server_socket_.setNonBlocking()) {
            logger_->error("Failed to set non blocking in start()");
            return false;
        }

        if (!server_socket_.bindSocket(PORT)) {
            logger_->error("Failed to bind socket");
            return false;
        }

        if (!server_socket_.listenSocket(BACKLOG)) {
            logger_->error("Failed to listen socket");
            return false;
        }

        if (!epoll_.create()) {
            logger_->error("Failed to create epoll");
            return false;
        }

        if (!epoll_.add(server_socket_.getFd(), EpollEvents::IN | EpollEvents::ET)) {
            logger_->error("Failed to add events in start()");
            return false;
        }

        logger_->info("Server started");
        running_ = true;
        return true;
    }

    void run() {
        while (running_) {
            auto events = epoll_.wait();
            if (events.empty() && !running_) {
                break;
            }

            for (const auto& event : events) {
                int fd = event.data.fd;
                if (fd == server_socket_.getFd()) {
                    handleNewConnection();
                } else {
                    if (event.events & EpollEvents::IN) {
                        handleClientData(fd);
                    }
                    if (event.events & EpollEvents::ERR) {
                        handleClientError(fd);
                    }
                    if (event.events & EpollEvents::HUP) {
                        handleClientDisconnect(fd);
                    }
                }
            }
        }
    }

    void stop() {
        if (running_) {
            running_ = false;
            for (auto& client : clients_) {
                client.second->close();
            }
            clients_.clear();
            server_socket_.close();
            epoll_.close();
            logger_->info("Server stopped");
        }
    }

private:
    void handleNewConnection() {
        while (true) {
            Socket client_socket = server_socket_.acceptSocket();

            if (!client_socket.isValid()) {
                break;
            }

            int client_fd = client_socket.getFd();

            if (!client_socket.setNonBlocking(true)) {
                logger_->error("Failed to set non blocking in handleNewConnection()");
                client_socket.close();
                continue;
            }

            if (!epoll_.add(client_fd, EpollEvents::IN | EpollEvents::ET | EpollEvents::HUP | EpollEvents::ERR)) {
                logger_->error("Failed to add events in handleNewConnection()");
                client_socket.close();
                continue;
            }

            std::string peer_addr = client_socket.getPeerAddress();
            int peer_port = client_socket.getPeerPort();
            clients_[client_fd] = std::make_unique<Socket>(std::move(client_socket));
            logger_->info("New connection accepted from {}:{}", peer_addr, peer_port);
        }
    }

    void handleClientData(int client_fd) {
        auto it = clients_.find(client_fd);
        if (it == clients_.end()) {
            return;
        }

        Socket& client_socket = *it->second;
        std::vector<char> buffer(4096);

        while (true) {
            ssize_t bytes_read = client_socket.recv(buffer, 4096);

            if (bytes_read > 0) {
                std::string recv_data(buffer.begin(), buffer.begin() + bytes_read);
                std::string peer_addr = client_socket.getPeerAddress();
                int peer_port = client_socket.getPeerPort();
                logger_->info("Received from {}:{} : {}", peer_addr, peer_port, recv_data);

                ssize_t bytes_sent = client_socket.send(recv_data);
                if (bytes_sent < 0) {
                    logger_->error("Failed to send data to client");
                    handleClientDisconnect(client_fd);
                    break;
                }
            } else if (bytes_read == 0) {
                handleClientDisconnect(client_fd);
                break;
            } else if (bytes_read == -1) {
                if (errno == EAGAIN && errno == EWOULDBLOCK) {
                    break;
                } else {
                    handleClientError(client_fd);
                    break;
                }
            }
        }
    }

    void handleClientError(int client_fd) {
        logger_->error("Connection Error");
        handleClientDisconnect(client_fd);
    }

    void handleClientDisconnect(int client_fd) {
        auto it = clients_.find(client_fd);
        if (it != clients_.end()) {
            logger_->info("Client disconnected");
            epoll_.remove(client_fd);
            clients_.erase(it);
        }
    }
};

volatile sig_atomic_t stop_server = 0;

void signalHandler(int signum) {
    std::cout << "\nReceived signal: " << signum << std::endl;
    stop_server = 1;
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    EpollServer server;

    if (!server.start()) {
        std::cerr << "Error starting server" << std::endl;
        return -1;
    }

    std::cout << "Server is running..." << std::endl;

    server.run();

    server.stop();
    std::cout << "Server stopped" << std::endl;

    return 0;
}