#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace dobot::sdk {

namespace detail {

// 一条订阅在 SDK 内部的状态。dobot_dds 的 worker 更新 latest，再调用 On()。
template <typename Msg>
struct SubState {
    mutable std::mutex mu;
    std::condition_variable cv;
    std::optional<Msg> latest;
    std::function<void(const Msg&)> on;
    std::function<int()> matched_count;
};

}  // namespace detail

// 一条状态订阅。拷贝出来的对象看的是同一条订阅。
// 周期控制读 Latest()。急停、故障这类事件再用 On()。
template <typename Msg>
class Subscriber {
public:
    // 还没有数据时返回空。缺失不会变成数值 0。
    std::optional<Msg> Latest() const {
        if (!state_) {
            return std::nullopt;
        }
        std::lock_guard<std::mutex> lock(state_->mu);
        return state_->latest;
    }

    // 当前匹配上的对端数量。0 表示还没和对端连上。
    int Matched() const {
        if (!state_ || !state_->matched_count) {
            return 0;
        }
        return state_->matched_count();
    }

    // 等到第一帧有效数据，或超时。已经有数据时立刻返回 true。
    bool WaitForData(std::chrono::milliseconds timeout) const {
        if (!state_) {
            return false;
        }
        std::unique_lock<std::mutex> lock(state_->mu);
        return state_->cv.wait_for(lock, timeout, [&] { return state_->latest.has_value(); });
    }

    // 事件回调。跑在 dobot_dds 的有序 worker 上，不跑在 DDS 收取线程上。
    // 回调里不要调用 Shutdown、不要再订话题、不要调用 RPC，也不要长时间卡住。
    void On(std::function<void(const Msg&)> callback) {
        if (!state_) {
            return;
        }
        std::lock_guard<std::mutex> lock(state_->mu);
        state_->on = std::move(callback);
    }

    bool valid() const { return static_cast<bool>(state_); }

private:
    friend class Session;

    std::shared_ptr<detail::SubState<Msg>> state_;
};

// 一条命令发布。控制环里直接 Write，命令不排队，新的一帧盖住旧的一帧。
// Write 返回 true 只表示本地交给了 DDS，不表示机器人已经执行。
template <typename Msg>
class Publisher {
public:
    bool Write(const Msg& msg) const;
    int Matched() const;
    bool valid() const { return static_cast<bool>(state_); }

private:
    struct State;
    friend class Session;

    std::shared_ptr<State> state_;
};

}  // namespace dobot::sdk
