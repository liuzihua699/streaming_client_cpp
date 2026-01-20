#pragma once
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <poll.h>
#include <unistd.h>
#include "util/SockException.h"
#include "util/SockUtil.h"
#include "util/Buffer.h"

namespace toolkit {

/**
 * TCP客户端基类（参考ZLToolKit的TcpClient）
 * 子类需要重写 onConnect, onRecv, onError 回调
 */
class TcpClient : public std::enable_shared_from_this<TcpClient> {
public:
    using Ptr = std::shared_ptr<TcpClient>;

    TcpClient() = default;
    virtual ~TcpClient() { shutdown(); }

    /**
     * 开始连接TCP服务器
     * @param host 服务器IP或域名
     * @param port 服务器端口
     * @param timeout_sec 超时时间（秒）
     */
    virtual void startConnect(const std::string& host, uint16_t port, float timeout_sec = 5.0f) {
        shutdown();

        _host = host;
        _port = port;

        // 异步连接
        _fd = SockUtil::connect(host.c_str(), port, true);
        if (_fd < 0) {
            onConnect(SockException(Err_dns, "connect failed"));
            return;
        }

        // 等待连接完成
        struct pollfd pfd{};
        pfd.fd = _fd;
        pfd.events = POLLOUT;

        int ret = poll(&pfd, 1, (int)(timeout_sec * 1000));
        if (ret <= 0) {
            ::close(_fd);
            _fd = -1;
            onConnect(SockException(Err_timeout, "connect timeout"));
            return;
        }

        // 检查连接结果
        int err = SockUtil::getSockError(_fd);
        if (err != 0) {
            ::close(_fd);
            _fd = -1;
            onConnect(SockException(Err_refused, strerror(err)));
            return;
        }

        // 连接成功，启动接收线程
        _running = true;
        _recv_thread = std::thread([this]() { recvLoop(); });

        onConnect(SockException(Err_success, "success"));
    }

    /**
     * 发送数据
     */
    ssize_t send(const std::string& data) {
        return send(data.data(), data.size());
    }

    ssize_t send(const char* data, size_t len) {
        if (_fd < 0 || !_running) return -1;
        return ::send(_fd, data, len, MSG_NOSIGNAL);
    }

    ssize_t send(const Buffer::Ptr& buf) {
        return send(buf->data(), buf->size());
    }

    /**
     * 主动断开连接
     */
    void shutdown(const SockException& ex = SockException(Err_shutdown, "self shutdown")) {
        if (!_running && _fd < 0) return;

        _running = false;

        if (_fd >= 0) {
            ::shutdown(_fd, SHUT_RDWR);
            ::close(_fd);
            _fd = -1;
        }

        // 等待接收线程结束
        if (_recv_thread.joinable()) {
            if (_recv_thread.get_id() != std::this_thread::get_id()) {
                _recv_thread.join();
            } else {
                _recv_thread.detach();
            }
        }
    }

    /**
     * 是否已连接
     */
    bool alive() const { return _fd >= 0 && _running; }

    /**
     * 获取本地IP
     */
    std::string getLocalIP() const {
        return _fd >= 0 ? SockUtil::getLocalIP(_fd) : "";
    }

    /**
     * 获取对端IP
     */
    std::string getPeerIP() const {
        return _fd >= 0 ? SockUtil::getPeerIP(_fd) : "";
    }

protected:
    /**
     * 连接结果回调（子类必须重写）
     */
    virtual void onConnect(const SockException& ex) = 0;

    /**
     * 收到数据回调（子类必须重写）
     */
    virtual void onRecv(const Buffer::Ptr& buf) = 0;

    /**
     * 连接断开回调
     */
    virtual void onError(const SockException& ex) {
        // 默认空实现，子类可重写
    }

private:
    void recvLoop() {
        auto buf = std::make_shared<BufferRaw>();
        buf->setCapacity(64 * 1024);  // 64KB接收缓冲

        while (_running && _fd >= 0) {
            struct pollfd pfd{};
            pfd.fd = _fd;
            pfd.events = POLLIN | POLLERR | POLLHUP;

            int ret = poll(&pfd, 1, 100);  // 100ms超时
            if (ret < 0) {
                if (errno == EINTR) continue;
                onError(SockException(Err_other, strerror(errno)));
                break;
            }

            if (ret == 0) continue;  // 超时

            if (pfd.revents & (POLLERR | POLLHUP)) {
                onError(SockException(Err_eof, "connection closed"));
                break;
            }

            if (pfd.revents & POLLIN) {
                ssize_t n = ::recv(_fd, buf->data(), buf->getCapacity(), 0);
                if (n > 0) {
                    buf->setSize(n);
                    onRecv(buf);
                } else if (n == 0) {
                    onError(SockException(Err_eof, "peer closed"));
                    break;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                        continue;
                    }
                    onError(SockException(Err_other, strerror(errno)));
                    break;
                }
            }
        }
    }

protected:
    int _fd = -1;
    std::string _host;
    uint16_t _port = 0;

private:
    std::atomic<bool> _running{false};
    std::thread _recv_thread;
};

} // namespace toolkit
