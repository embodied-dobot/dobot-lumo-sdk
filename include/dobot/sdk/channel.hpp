#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace dobot::sdk {

namespace detail {

// Internal state of one subscription in the SDK. A dobot_dds worker updates latest, then calls On().
template <typename Msg>
struct SubState {
    mutable std::mutex mu;
    std::condition_variable cv;
    std::optional<Msg> latest;
    std::function<void(const Msg&)> on;
    std::function<int()> matched_count;
};

}  // namespace detail

// One state subscription. Copies refer to the same subscription.
// Periodic control reads Latest(). Use On() for events such as e-stop and faults.
template <typename Msg>
class Subscriber {
public:
    // Returns empty when no data has arrived. Missing data is never turned into the value 0.
    std::optional<Msg> Latest() const {
        if (!state_) {
            return std::nullopt;
        }
        std::lock_guard<std::mutex> lock(state_->mu);
        return state_->latest;
    }

    // Number of currently matched peers. 0 means not yet connected to any peer.
    int Matched() const {
        if (!state_ || !state_->matched_count) {
            return 0;
        }
        return state_->matched_count();
    }

    // Wait for the first valid sample, or time out. Returns true immediately if data is already available.
    bool WaitForData(std::chrono::milliseconds timeout) const {
        if (!state_) {
            return false;
        }
        std::unique_lock<std::mutex> lock(state_->mu);
        return state_->cv.wait_for(lock, timeout, [&] { return state_->latest.has_value(); });
    }

    // Event callback. Runs on an ordered dobot_dds worker, not on the DDS receive thread.
    // Inside the callback, do not call Shutdown, subscribe to more topics, or call RPC, and do not block for long.
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

// One command publisher. Call Write directly in the control loop; commands are not queued, and a new sample overwrites the previous one.
// Write returning true only means the sample was handed to DDS locally, not that the robot has executed it.
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
