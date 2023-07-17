//
// Created by Shawn Zhao on 2023/7/14.
//

#ifndef LIGHT_THREAD_POOL_LEARNING_CANCEL_H
#define LIGHT_THREAD_POOL_LEARNING_CANCEL_H
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "status.h"
#include "visibility.h"

namespace arrow {
class StopToken;

struct StopSourceImpl;

//// EXPERIMENTAL
class ARROW_EXPORT StopSource {
public:
    StopSource();
    ~StopSource();

    // Consumer API (the side that stops)
    void RequestStop();
    void RequestStop(Status error);
    void RequestStopFromSignal(int signum);

    StopToken token();

    // For internal use only
    void Reset();

protected:
    std::shared_ptr<StopSourceImpl> impl_; // shared_ptr 智能指针
};

class ARROW_EXPORT StopToken {
public:
    StopToken() {}
    // direct change owning power from impl to impl_
    explicit StopToken(std::shared_ptr<StopSourceImpl> impl) : impl_(std::move(impl)) {}

    // A trivial token that never propagates any stop request
    static StopToken Unstoppable() { return StopToken(); }

    // Producer API (the side that gets asked to stopped)
    Status Poll() const;
    bool IsStopRequested() const;

protected:
    std::shared_ptr<StopSourceImpl> impl_;
};
}
#endif //LIGHT_THREAD_POOL_LEARNING_CANCEL_H
