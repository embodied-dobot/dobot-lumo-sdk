#pragma once

// per-topic SPSC 队列 + worker 池；同 topic 内 FIFO，跨 topic 可并行

#include <thread/readerwriterqueue.h>
#include <thread/utils.hpp>

#include <dobot_log.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace dobot::dds
{

    using WorkerWakeSemaphore = moodycamel::spsc_sema::LightweightSemaphore;

    struct OrderedLaneDispatch {
        // 函数指针 dispatch，避免 std::function 间接调用开销
        bool (*try_dispatch)(void *ctx){nullptr};
        void *ctx{nullptr};

        bool poll() const { return try_dispatch != nullptr && try_dispatch(ctx); }
    };

    template <typename Sample>
    class OrderedReceiveLane : public std::enable_shared_from_this<OrderedReceiveLane<Sample>>
    {
    public:
        using Callback = std::function<void(Sample)>;

        // 单生产者单消费者；队列满则丢弃并计数 overflow
        OrderedReceiveLane(size_t capacity, Callback callback, std::string lane_name) : queue_(capacity > 0 ? capacity : 4096), callback_(std::move(callback)), lane_name_(std::move(lane_name)) {}

        void startAccepting() noexcept { accepting_.store(true, std::memory_order_release); }
        void stopAccepting() noexcept { accepting_.store(false, std::memory_order_release); }

        bool push(Sample sample)
        {
            if (!accepting_.load(std::memory_order_acquire)) { return false; }
            if (queue_.try_enqueue(std::move(sample))) {
                if (enqueue_wake_) { enqueue_wake_(); }
                return true;
            }
            DOBOT_WARN("OrderedReceiveLane[{}]: queue full, dropped (overflow={})", lane_name_, ++overflow_);
            return false;
        }

        void setEnqueueWake(std::function<void()> wake) { enqueue_wake_ = std::move(wake); }

        bool tryDispatchOne() { return dispatchOne(); }

        void drainRemaining()
        {
            while (dispatchOne()) {}
        }

        OrderedLaneDispatch makeDispatchSlot()
        {
            auto self = this->shared_from_this();
            return OrderedLaneDispatch{[](void *ctx) -> bool { return static_cast<OrderedReceiveLane *>(ctx)->tryDispatchOne(); }, self.get()};
        }

        uint64_t overflowCount() const noexcept { return overflow_.load(std::memory_order_relaxed); }
        const std::string &laneName() const noexcept { return lane_name_; }

    private:
        bool dispatchOne()
        {
            Sample item;
            if (!queue_.try_dequeue(item)) { return false; }
            callback_(std::move(item));
            return true;
        }

        moodycamel::ReaderWriterQueue<Sample> queue_;
        Callback callback_;
        std::string lane_name_;
        std::function<void()> enqueue_wake_;
        std::atomic<bool> accepting_{false};
        std::atomic<uint64_t> overflow_{0};
    };

    class OrderedDispatchPool
    {
    public:
        static constexpr size_t kMaxLanesPerWorker = 512;

        explicit OrderedDispatchPool(size_t worker_count = 4) : worker_count_(worker_count > 0 ? worker_count : 1), workers_(std::make_unique<WorkerState[]>(worker_count_)) {}

        OrderedDispatchPool(const OrderedDispatchPool &) = delete;
        OrderedDispatchPool &operator=(const OrderedDispatchPool &) = delete;
        ~OrderedDispatchPool() { stop(); }

        size_t workerCount() const noexcept { return worker_count_; }

        std::function<void()> workerWakeNotifier(const std::string &topic_name)
        {
            const size_t wid = workerIndexForTopic(topic_name);
            return [this, wid] { notifyWorker(wid); };
        }

        void registerLane(OrderedLaneDispatch slot, const std::string &topic_name)
        {
            const size_t wid = workerIndexForTopic(topic_name);
            auto &ws = workers_[wid];
            std::lock_guard<std::mutex> lock(register_mutex_);
            const size_t idx = ws.lane_count.load(std::memory_order_relaxed);
            if (idx >= kMaxLanesPerWorker) {
                DOBOT_WARN("OrderedDispatchPool: worker {} full (max={})", wid, kMaxLanesPerWorker);
                return;
            }
            ws.slots[idx] = slot;
            ws.lane_count.store(idx + 1, std::memory_order_release);
        }

        void start()
        {
            bool expected = false;
            if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) { return; }
            threads_.reserve(worker_count_);
            for (size_t i = 0; i < worker_count_; ++i) {
                threads_.emplace_back([this, i] { workerLoop(i); });
            }
            DOBOT_INFO("OrderedDispatchPool started, workers={}", worker_count_);
        }

        void stop() noexcept
        {
            if (!running_.exchange(false, std::memory_order_acq_rel)) { return; }
            for (size_t i = 0; i < worker_count_; ++i) {
                workers_[i].idle.store(false, std::memory_order_release);
                workers_[i].wake_sem.signal();
            }
            for (auto &t : threads_) {
                if (t.joinable()) { t.join(); }
            }
            threads_.clear();
        }

    private:
        size_t workerIndexForTopic(const std::string &topic_name) const noexcept { return std::hash<std::string>{}(topic_name) % worker_count_; }

        void notifyWorker(size_t worker_id) noexcept
        {
            // worker 空闲时才 signal，合并唤醒
            WorkerState &ws = workers_[worker_id];
            if (ws.idle.load(std::memory_order_acquire)) { ws.wake_sem.signal(); }
        }

        struct alignas(64) WorkerState {
            std::array<OrderedLaneDispatch, kMaxLanesPerWorker> slots{};
            std::atomic<size_t> lane_count{0};
            std::atomic<bool> idle{true};
            WorkerWakeSemaphore wake_sem{0};
        };

        static bool pollAll(WorkerState &ws, size_t lane_count)
        {
            bool work = false;
            for (size_t i = 0; i < lane_count; ++i) {
                while (ws.slots[i].poll()) { work = true; }
            }
            return work;
        }

        void workerLoop(size_t worker_id)
        {
            dobot::utils::set_current_thread_name("ord_disp_" + std::to_string(worker_id));
            WorkerState &ws = workers_[worker_id];

            while (running_.load(std::memory_order_acquire)) {
                const size_t lane_count = ws.lane_count.load(std::memory_order_acquire);
                if (lane_count == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                if (pollAll(ws, lane_count)) { continue; }

                // 标记 idle 后再 wait，避免丢唤醒
                ws.idle.store(true, std::memory_order_release);
                if (pollAll(ws, lane_count)) {
                    ws.idle.store(false, std::memory_order_release);
                    continue;
                }
                ws.wake_sem.wait();
                ws.idle.store(false, std::memory_order_release);
            }
        }

        size_t worker_count_;
        std::atomic<bool> running_{false};
        std::mutex register_mutex_;
        std::unique_ptr<WorkerState[]> workers_;
        std::vector<std::thread> threads_;
    };

}  // namespace dobot::dds
