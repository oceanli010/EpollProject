#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "../../include/socket.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"  // 彩色控制台
#include "spdlog/sinks/basic_file_sink.h"     // 文件日志

class EchoClient {
private:
    Socket socket_;
    std::string server_ip_;
    int server_port_;
    bool connected_;
    std::shared_ptr<spdlog::logger> logger_;

public:
    EchoClient(const std::string &server_ip, int server_port): server_ip_(server_ip), server_port_(server_port), connected_(false) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/server.log", true);

        std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("client_logger", sinks.begin(), sinks.end());
        logger_->set_level(spdlog::level::info);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
    }

    ~EchoClient() {
        disconnect();
    }

    bool connect() {
        if (!socket_.createSocket()) {
            logger_->error("Failed to create socket");
            return false;
        }

        if (!socket_.connect(server_ip_, server_port_)) {
            logger_->error("Failed to connect to server");
            return false;
        }

        logger_->info("Connected to server");
        connected_ = true;
        return true;
    }

    void disconnect() {
        if (connected_) {
            socket_.close();
            connected_ = false;
            logger_->error("Disconnected from server");
        }
    }

    bool sendMessage(const std::string& message) {
        if (!connected_) {
            logger_->error("Failed to send message: connection lost");
            return false;
        }

        ssize_t bytes_sent = socket_.send(message);
        if (bytes_sent < 0) {
            logger_->error("Failed to send message: no data send");
            return false;
        }

        logger_->info("Send {} byte(s)", bytes_sent);
        return true;
    }

    bool receiveResponse() {
        if (!connected_) {
            logger_->error("Failed to receive message: connection lost");
            return false;
        }

        std::vector<char> buffer;
        ssize_t bytes_received = socket_.recv(buffer, 4096);

        if (bytes_received > 0) {
            std::string response(buffer.begin(), buffer.end());
            logger_->info("Received: {} ({} bytes)", response, bytes_received);
            return true;
        } else if (bytes_received == 0) {
            logger_->error("Connection closed");
            return false;
        } else {
            logger_->error("Failed to connect to server");
            return false;
        }
    }

    bool isConnected() const {
        return connected_;
    }

    void interactiveMode() {
        if (!connected_) {
            logger_->error("Failed to connect to server");
        }

        std::cout << "\n=== Echo Client Interactive Mode ===" << std::endl;
        std::cout << "Type 'quit' to exit, or enter message to send" << std::endl;

        std::string input;
        while (connected_) {
            std::cout << "Enter message: ";
            std::getline(std::cin, input);

            if (input == "quit") {
                break;
            }

            if (!sendMessage(input + "\n")) {
                break;
            }
            if (!receiveResponse()) {
                break;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;

    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = std::atoi(argv[2]);
    }

    std::cout << "Echo Client Starting..." << std::endl;
    std::cout << "Server IP: " << server_ip << ":" << server_port << std::endl;

    EchoClient client(server_ip, server_port);

    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return -1;
    }

    std::cout << "\nTesting connection..." << std::endl;
    if (client.sendMessage("Hello World!")) {
        client.receiveResponse();
    }
    client.interactiveMode();

    client.disconnect();
    std::cout << "Client shutdown complete" << std::endl;
    return 0;
}