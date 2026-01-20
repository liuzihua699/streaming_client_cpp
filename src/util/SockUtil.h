#pragma once
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

namespace toolkit {

// Socket工具类（参考ZLToolKit的sockutil）
class SockUtil {
public:
    /**
     * 创建TCP连接
     * @param host 主机名或IP
     * @param port 端口
     * @param async 是否异步连接
     * @return socket fd，失败返回-1
     */
    static int connect(const char* host, uint16_t port, bool async = true) {
        sockaddr_storage addr;
        if (!getDomainIP(host, port, addr)) {
            return -1;
        }

        int sockfd = socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd < 0) {
            return -1;
        }

        setReuseable(sockfd);
        setNoBlocked(sockfd, async);
        setNoDelay(sockfd);
        setCloExec(sockfd);

        if (::connect(sockfd, (sockaddr*)&addr, getSockLen((sockaddr*)&addr)) == 0) {
            return sockfd;
        }

        if (async && (errno == EINPROGRESS || errno == EAGAIN)) {
            return sockfd;
        }

        ::close(sockfd);
        return -1;
    }

    /**
     * DNS解析
     */
    static bool getDomainIP(const char* host, uint16_t port, sockaddr_storage& addr) {
        memset(&addr, 0, sizeof(addr));

        // 先尝试直接解析IP
        auto* addr4 = (sockaddr_in*)&addr;
        if (inet_pton(AF_INET, host, &addr4->sin_addr) == 1) {
            addr4->sin_family = AF_INET;
            addr4->sin_port = htons(port);
            return true;
        }

        // DNS解析
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host, nullptr, &hints, &result) != 0 || !result) {
            return false;
        }

        memcpy(&addr, result->ai_addr, result->ai_addrlen);
        ((sockaddr_in*)&addr)->sin_port = htons(port);
        freeaddrinfo(result);
        return true;
    }

    /**
     * 设置非阻塞
     */
    static int setNoBlocked(int fd, bool noblock = true) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) return -1;
        if (noblock) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }
        return fcntl(fd, F_SETFL, flags);
    }

    /**
     * 设置地址复用
     */
    static int setReuseable(int fd, bool reuse = true) {
        int opt = reuse ? 1 : 0;
        return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    /**
     * 设置TCP_NODELAY（禁用Nagle算法）
     */
    static int setNoDelay(int fd, bool nodelay = true) {
        int opt = nodelay ? 1 : 0;
        return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }

    /**
     * 设置FD_CLOEXEC
     */
    static int setCloExec(int fd, bool cloexec = true) {
        int flags = fcntl(fd, F_GETFD, 0);
        if (flags < 0) return -1;
        if (cloexec) {
            flags |= FD_CLOEXEC;
        } else {
            flags &= ~FD_CLOEXEC;
        }
        return fcntl(fd, F_SETFD, flags);
    }

    /**
     * 获取socket错误
     */
    static int getSockError(int fd) {
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            return errno;
        }
        return error;
    }

    /**
     * 获取sockaddr长度
     */
    static socklen_t getSockLen(const sockaddr* addr) {
        switch (addr->sa_family) {
            case AF_INET: return sizeof(sockaddr_in);
            case AF_INET6: return sizeof(sockaddr_in6);
            default: return 0;
        }
    }

    /**
     * 获取本地IP
     */
    static std::string getLocalIP(int fd) {
        sockaddr_storage addr{};
        socklen_t len = sizeof(addr);
        if (getsockname(fd, (sockaddr*)&addr, &len) < 0) {
            return "";
        }
        char buf[INET6_ADDRSTRLEN] = {0};
        if (addr.ss_family == AF_INET) {
            inet_ntop(AF_INET, &((sockaddr_in*)&addr)->sin_addr, buf, sizeof(buf));
        } else {
            inet_ntop(AF_INET6, &((sockaddr_in6*)&addr)->sin6_addr, buf, sizeof(buf));
        }
        return buf;
    }

    /**
     * 获取对端IP
     */
    static std::string getPeerIP(int fd) {
        sockaddr_storage addr{};
        socklen_t len = sizeof(addr);
        if (getpeername(fd, (sockaddr*)&addr, &len) < 0) {
            return "";
        }
        char buf[INET6_ADDRSTRLEN] = {0};
        if (addr.ss_family == AF_INET) {
            inet_ntop(AF_INET, &((sockaddr_in*)&addr)->sin_addr, buf, sizeof(buf));
        } else {
            inet_ntop(AF_INET6, &((sockaddr_in6*)&addr)->sin6_addr, buf, sizeof(buf));
        }
        return buf;
    }
};

} // namespace toolkit
