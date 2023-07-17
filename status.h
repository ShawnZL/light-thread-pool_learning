//
// Created by Shawn Zhao on 2023/7/14.
//

#ifndef LIGHT_THREAD_POOL_LEARNING_STATUS_H
#define LIGHT_THREAD_POOL_LEARNING_STATUS_H
#include <cstring>
#include <iostream>

enum class StatusCode {INVALID = -1, OK, Cancelled, KeyError};

class Status {
public:
    Status(): code_(StatusCode::OK) {} // 构造函数
    explicit Status(StatusCode code): code_(code) {}
    //explicit只能使用显式转换 阻止行为Status x = Status(code);
    Status(StatusCode code, const std::string& msg): code_(code), msg_(msg) {}

    StatusCode code() const { return code_; }

    const std::string& message() const { return msg_; }

    bool ok() const { return code_ == StatusCode::OK; }

    static Status OK() { return Status(); } // 静态成员，供所有成员使用

    static Status Invalid(const std::string& msg) {
        return Status(StatusCode::INVALID, msg);
    }

    static Status Cancelled(const std::string& msg) {
        return Status(StatusCode::Cancelled, msg);
    }

    static Status KeyError(const std::string& msg) {
        return Status(StatusCode::KeyError, msg);
    }

    std::string ToString() const {
        std::string statusString;

        switch (code_) {
            case StatusCode::OK:
                statusString = "OK";
                break;
            case StatusCode::Cancelled:
                statusString = "Cancelled";
                break;
            default:
                statusString = "Unknown";
                break;
        }
        if (!msg_.empty()) {
            statusString += ": " + msg_;
        }

        return statusString;
    }

    void Abort(const std::string& message) const {
        // std::cerr 标准错误输出流，不需刷新
        std::cerr << "-- Arrow Fatal Error --\n";
        if (!message.empty()) {
            std::cerr << message << "\n";
        }
        std::cerr << ToString() << std::endl;
        std::abort(); // 终止API
    }

private:
    StatusCode code_;
    std::string msg_;
};

#endif //LIGHT_THREAD_POOL_LEARNING_STATUS_H
