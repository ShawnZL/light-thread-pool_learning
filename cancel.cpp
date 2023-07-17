//
// Created by Shawn Zhao on 2023/7/14.
// 用于提供取消操作的支持

#include "cancel.h"
#include <atomic>
#include <mutex>
#include <utility>
#include "macros.h"

namespace arrow {

#if ATOMIC_INT_LOCK_FREE != 2
#error Lock-free atomic int required for signal safety
#endif
// NOTE: We care mainly about the making the common case (not cancelled) fast.
struct StopSourceImpl {
    // 用于存储取消操作的状态信息。它包含了一个原子整数 requested_
    // 和一个互斥锁 mutex_，以及一个用于存储取消错误的 Status 对象。
    std::atomic<int> requested_{0};  // will be -1 or signal number if requested
    // 如果 requested_ 的值为 0，表示取消操作没有被请求。
    // 如果 requested_ 的值为 -1，表示取消操作已经被请求。
    // 如果 requested_ 的值大于 0，表示取消操作由特定的信号（signal）引发，该值代表了信号的编号。
    std::mutex mutex_;
    Status cancel_error_;
};

StopSource::StopSource(): impl_(new StopSourceImpl) {}

StopSource::~StopSource() = default;

// 请求取消操作
void StopSource::RequestStop() {
    RequestStop(Status::Cancelled("Operation cancelled"));
}

void StopSource::RequestStop(Status st) {
    // 在作用域控制对象所有权的类型
    std::lock_guard<std::mutex> lock(impl_ -> mutex_);

    DCHECK_NOT_OK(st);
    if (!impl_ -> requested_) {
        impl_ -> requested_ = -1;
        impl_ -> cancel_error_ = std::move(st); // 转换所有权
    }
}

void StopSource::RequestStopFromSignal(int signum) {
    impl_ -> requested_.store(signum); // 存储signum
}

// 重新启动
void StopSource::Reset() {
    std::lock_guard<std::mutex> lock(impl_ -> mutex_);
    impl_ -> cancel_error_ = Status::OK(); // 返回Status成员
    impl_ -> requested_.store(0);
}

// StopToken于表示一个取消令牌，可以用于检查是否请求了取消操作，并通过 Poll() 函数来获取取消错误的状态。

StopToken StopSource::token() { return StopToken(impl_); }

bool StopToken::IsStopRequested() const {
    if (!impl_) {
        return false;
    }
    return impl_ -> requested_.load() != 0;
}

Status StopToken::Poll() const {
    if (!impl_) {
        return Status::OK(); // 返回Status成员
    }
    if (!impl_->requested_.load()) {
        return Status::OK(); // 返回Status成员
    }

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->cancel_error_.ok()) { // 如果statuscode不等于OK
        auto signum = impl_->requested_.load();
        DCHECK_GE(signum, 0); // 断言不成立，离开终止程序
        impl_->cancel_error_ = Status::Cancelled("Operation cancelled");
    }
    return impl_->cancel_error_;
}

} // namespace arrow

