#pragma once
#include <string>
#include <exception>
#include <ostream>

namespace toolkit {

// 错误类型枚举（参考ZLToolKit）
enum ErrCode {
    Err_success = 0,    // 成功
    Err_eof,            // 连接关闭
    Err_timeout,        // 超时
    Err_refused,        // 连接被拒绝
    Err_reset,          // 连接被重置
    Err_dns,            // DNS解析失败
    Err_shutdown,       // 主动关闭
    Err_other = 0xFF    // 其他错误
};

// Socket异常类（参考ZLToolKit）
class SockException : public std::exception {
public:
    SockException(ErrCode code = Err_success, const std::string& msg = "", int custom_code = 0)
        : _code(code), _custom_code(custom_code), _msg(msg) {}

    void reset(ErrCode code, const std::string& msg, int custom_code = 0) {
        _code = code;
        _msg = msg;
        _custom_code = custom_code;
    }

    const char* what() const noexcept override {
        return _msg.c_str();
    }

    ErrCode getErrCode() const { return _code; }
    int getCustomCode() const { return _custom_code; }

    // 判断是否有错误
    operator bool() const { return _code != Err_success; }

private:
    ErrCode _code;
    int _custom_code;
    std::string _msg;
};

inline std::ostream& operator<<(std::ostream& os, const SockException& ex) {
    return os << ex.getErrCode() << "(" << ex.what() << ")";
}

} // namespace toolkit
