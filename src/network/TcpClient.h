#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

class TcpClient : public std::enable_shared_from_this<TcpClient> {
public:
    using Ptr = std::shared_ptr<TcpClient>;

    virtual ~TcpClient() { shutdown(); }

    bool connect(const std::string& host, uint16_t port, float timeout = 5.0f) {
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) return false;

        int flags = fcntl(_fd, F_GETFL, 0);
        fcntl(_fd, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        struct hostent* he = gethostbyname(host.c_str());
        if (he) memcpy(&addr.sin_addr, he->h_addr, he->h_length);
        else inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        ::connect(_fd, (sockaddr*)&addr, sizeof(addr));

        struct pollfd pfd{_fd, POLLOUT, 0};
        if (poll(&pfd, 1, timeout * 1000) <= 0) {
            close(_fd); _fd = -1;
            return false;
        }

        _running = true;
        _thread = std::thread([this]() { recvLoop(); });
        onConnect();
        return true;
    }

    ssize_t send(const std::string& data) {
        if (_fd < 0) return -1;
        return ::send(_fd, data.data(), data.size(), 0);
    }

    void shutdown() {
        if (!_running) return;
        _running = false;
        if (_fd >= 0) {
            ::shutdown(_fd, SHUT_RDWR);
            ::close(_fd);
            _fd = -1;
        }
        // 只有不是当前线程才能join
        if (_thread.joinable() && _thread.get_id() != std::this_thread::get_id()) {
            _thread.join();
        } else if (_thread.joinable()) {
            _thread.detach();  // 如果是自己调用，detach
        }
    }

    bool alive() const { return _fd >= 0 && _running; }

protected:
    virtual void onConnect() = 0;
    virtual void onRecv(const char* data, size_t len) = 0;
    virtual void onDisconnect() {}

private:
    void recvLoop() {
        char buf[8192];
        while (_running && _fd >= 0) {
            struct pollfd pfd{_fd, POLLIN, 0};
            if (poll(&pfd, 1, 100) > 0 && (pfd.revents & POLLIN)) {
                ssize_t n = recv(_fd, buf, sizeof(buf), 0);
                if (n > 0) onRecv(buf, n);
                else { onDisconnect(); break; }
            }
        }
    }

    int _fd = -1;
    std::atomic<bool> _running{false};
    std::thread _thread;
};