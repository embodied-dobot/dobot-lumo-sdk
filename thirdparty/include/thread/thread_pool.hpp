// 工业级线程池：兼顾高性能、零空闲 CPU、可控内存

#pragma once

#include "concurrentqueue.h"
#include "utils.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace dobot::utils
{
    /// 队列溢出策略
    enum class OverflowPolicy {
        kBlock,    // enqueue 阻塞直到有空位（真正的 CV 等待，不是自旋）
        kDiscard,  // 丢弃新任务
    };

    /**
     * 工业级线程池。
     *
     * 高频路径（enqueue / dequeue）使用 moodycamel 无锁队列，
     * worker 空闲时通过 condition_variable 阻塞，零 CPU 占用。
     */
    class ThreadPool
    {
    public:
        using TaskFn = std::function<void()>;

        /**
         * @param num_threads  工作线程数。0 表示 hardware_concurrency()。
         * @param max_pending  最大排队任务数。0 表示不限制。
         * @param overflow     队列满时的策略。
         * @param node_pool_size 预分配任务节点数。0 则每次堆分配。
         */
        explicit ThreadPool(size_t num_threads = 0, size_t max_pending = 0, OverflowPolicy overflow = OverflowPolicy::kBlock, size_t node_pool_size = 1024)
          : max_pending_(max_pending), overflow_(overflow)
        {
            if (num_threads == 0) {
                num_threads = std::thread::hardware_concurrency();
                if (num_threads == 0) { num_threads = 4; }
            }

            num_workers_ = num_threads;
            workers_.reserve(num_workers_);
            consume_tokens_.reserve(num_workers_);

            for (size_t i = 0; i < num_workers_; ++i) {
                workers_.emplace_back(std::make_unique<WorkerData>());
                consume_tokens_.emplace_back(std::make_unique<moodycamel::ConsumerToken>(queue_));
            }

            // 预分配节点池并全部移交到 freelist_，确保 acquire_node 能真正复用
            for (size_t i = 0; i < node_pool_size; ++i) { freelist_.push_back(new Node{}); }

            running_.store(true, std::memory_order_release);

            for (size_t i = 0; i < num_workers_; ++i) {
                workers_[i]->thread = std::thread([this, i] { workerLoop(i); });
            }
        }

        ~ThreadPool() { shutdown(); }

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        // ---- 异步提交 ----

        /// 提交一个可调用对象，返回 std::future 获取结果。
        template <typename F, typename... Args>
        [[nodiscard]] auto submit(F &&f, Args &&...args) -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
        {
            using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
            using TaskType = std::packaged_task<ReturnType()>;

            auto task = std::make_shared<TaskType>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<ReturnType> result = task->get_future();

            if (!enqueue([task]() { (*task)(); })) { throw std::runtime_error("ThreadPool::submit: pool stopped or queue full"); }
            return result;
        }

        /// 提交无返回值的可调用对象。
        template <typename F, typename... Args>
        bool submit_detached(F &&f, Args &&...args)
        {
            return enqueue(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        }

        /// 提交一个 TaskFn（std::function<void()>）。
        bool post(TaskFn task) { return enqueue(std::move(task)); }

        // ---- 生命周期 ----

        /// 等待所有排队任务完成。
        void wait_all()
        {
            std::unique_lock<std::mutex> lk(idle_mutex_);
            idle_cv_.wait(lk, [this] { return pending_.load(std::memory_order_acquire) == 0; });
        }

        void waitIdle() { wait_all(); }

        /**
         * 优雅关闭。
         * @param timeout 等待任务完成的最大时间。0 表示无限等待。
         * @return true  所有任务已完成，正常关闭
         *         false 超时，部分任务未完成（此时内部已强制清理，
         *               调用方不应再调用 stop_now()）
         */
        bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
        {
            if (!running_.exchange(false, std::memory_order_acq_rel)) { return true; }

            wake_cv_.notify_all();

            if (timeout.count() > 0) {
                std::unique_lock<std::mutex> lk(idle_mutex_);
                if (!idle_cv_.wait_for(lk, timeout, [this] { return pending_.load(std::memory_order_acquire) == 0; })) {
                    // 超时
                    lk.unlock();
                    finish_shutdown();
                    return false;
                }
            } else {
                wait_all();
            }

            finish_shutdown();
            return true;
        }

        /// 立即停止：丢弃未执行的任务。可重复调用，仅首次生效。
        void stop_now()
        {
            if (!running_.exchange(false, std::memory_order_acq_rel)) { return; }
            finish_shutdown();
        }

        // ---- 查询 ----

        size_t num_workers() const noexcept { return num_workers_; }
        size_t threadCount() const noexcept { return num_workers_; }
        size_t num_pending_tasks() const noexcept { return pending_.load(std::memory_order_relaxed); }
        bool is_running() const noexcept { return running_.load(std::memory_order_acquire); }

    private:
        // 任务节点
        struct Node {
            TaskFn *task{nullptr};
        };

        struct alignas(64) WorkerData {
            std::thread thread;
        };

        Node *acquire_node()
        {
            std::lock_guard<std::mutex> lk(pool_mutex_);
            if (!freelist_.empty()) {
                auto *n = freelist_.back();
                freelist_.pop_back();
                return n;
            }
            // 池耗尽：堆分配兜底（在锁外分配，但这里保持简单）
            return new Node{};
        }

        void release_node(Node *node) noexcept
        {
            node->task = nullptr;
            std::lock_guard<std::mutex> lk(pool_mutex_);
            freelist_.push_back(node);
        }

        // ---- enqueue（核心高频路径） ----
       

        bool enqueue(TaskFn &&f)
        {
            if (!running_.load(std::memory_order_acquire)) { return false; }

            // 容量检查：kDiscard 直接返回，kBlock 用 CV 等待
            if (max_pending_ > 0) {
                if (overflow_ == OverflowPolicy::kDiscard) {
                    if (pending_.load(std::memory_order_acquire) >= max_pending_) { return false; }
                } else {
                    // kBlock：用条件变量等待（不是自旋！）
                    std::unique_lock<std::mutex> lk(space_mutex_);
                    space_cv_.wait(lk, [this] { return !running_.load(std::memory_order_acquire) || pending_.load(std::memory_order_acquire) < max_pending_; });
                    if (!running_.load(std::memory_order_acquire)) { return false; }
                }
            }

            auto *node = acquire_node();
            if (!node) { return false; }

            // 用 unique_ptr 保证异常安全（new TaskFn 可能抛出 bad_alloc）
            auto task_guard = std::make_unique<TaskFn>(std::move(f));
            node->task = task_guard.release();

            if (!running_.load(std::memory_order_acquire)) {
                delete node->task;
                release_node(node);
                return false;
            }

            // 先入队，再 +queued_（供 wake 条件变量）和 +pending_（供容量与 wait_all）；
            // 务必先入队再 bump 计数，保证 worker 被唤醒时队列中已可见数据。
            queue_.enqueue(node);
            (void)queued_.fetch_add(1, std::memory_order_release);
            (void)pending_.fetch_add(1, std::memory_order_release);
            wake_cv_.notify_one();
            return true;
        }

        // ---- workerLoop（核心高频路径） ----
      

        void workerLoop(size_t worker_id) noexcept
        {
            dobot::utils::set_current_thread_name(std::string("dobot_tpool_") + std::to_string(worker_id));
            auto &token = *consume_tokens_[worker_id];

            while (running_.load(std::memory_order_acquire)) {
                Node *node = nullptr;

                // 无锁快速出队（高频命中）
                if (queue_.try_dequeue(token, node)) [[likely]] {
                    (void)queued_.fetch_sub(1, std::memory_order_acq_rel);
                    execute(node);
                    continue;
                }

                if (!running_.load(std::memory_order_acquire)) { break; }

                std::unique_lock<std::mutex> lk(wake_mutex_);
                if (queue_.try_dequeue(token, node)) {
                    (void)queued_.fetch_sub(1, std::memory_order_acq_rel);
                    lk.unlock();
                    execute(node);
                    continue;
                }
                // shutdown 或队列里又有节点（与 enqueue 中 queued_ bump 配对）
                wake_cv_.wait(lk, [this] { return !running_.load(std::memory_order_acquire) || queued_.load(std::memory_order_acquire) > 0; });
                lk.unlock();
            }
        }

        void execute(Node *node) noexcept
        {
            if (!node || !node->task) {
                if (node) release_node(node);
                return;
            }

            // unique_ptr 保证异常时 delete task
            std::unique_ptr<TaskFn> task_guard(node->task);
            try {
                (*(node->task))();
            } catch (...) {
                // packaged_task 的 future 会捕获异常
            }

            release_node(node);

            const auto left = pending_.fetch_sub(1, std::memory_order_acq_rel);
            if (max_pending_ > 0 && left == max_pending_) {
                // 有 enqueue 在 space_cv_ 上等待队列空位
                std::lock_guard<std::mutex> lk(space_mutex_);
                space_cv_.notify_one();
            }
            if (left == 1) {
                // 最后一个任务完成，通知 wait_all
                std::lock_guard<std::mutex> lk(idle_mutex_);
                idle_cv_.notify_all();
            }
        }

        // ---- 关闭清理（低频路径） ----

        void finish_shutdown()
        {
            // 通知所有可能等待在 kBlock 策略下的生产者
            {
                std::lock_guard<std::mutex> lk(space_mutex_);
                space_cv_.notify_all();
            }

            wake_cv_.notify_all();

            for (auto &w : workers_) {
                if (w->thread.joinable()) { w->thread.join(); }
            }

            // 清理队列中剩余节点（未经 worker 出队，需扣回 queued_）
            {
                std::lock_guard<std::mutex> lk(pool_mutex_);
                Node *p = nullptr;
                while (queue_.try_dequeue(p)) {
                    if (p) {
                        (void)queued_.fetch_sub(1, std::memory_order_acq_rel);
                        delete p->task;
                        delete p;
                    }
                }
            }
            queued_.store(0, std::memory_order_release);

            pending_.store(0, std::memory_order_release);
            workers_.clear();
            consume_tokens_.clear();

            // 销毁 freelist 中所有节点（包括预分配的和运行时兜底堆分配的）
            {
                std::lock_guard<std::mutex> lk(pool_mutex_);
                for (auto *n : freelist_) { delete n; }
                freelist_.clear();
            }
        }

        // ---- 配置 ----
        size_t num_workers_{0};
        const size_t max_pending_;
        const OverflowPolicy overflow_;

        // ---- 无锁队列 ----
        moodycamel::ConcurrentQueue<Node *> queue_;
        std::vector<std::unique_ptr<moodycamel::ConsumerToken>> consume_tokens_;

        // ---- 任务节点池（mutex 保护，避免无锁 freelist 的 ABA 问题） ----
        std::mutex pool_mutex_;
        std::vector<Node *> freelist_;  // 当前空闲节点（含预分配+运行时兜底）

        // ---- 原子状态 ----
        std::atomic<bool> running_{false};
        /// 队列中待取出的节点数（与 ConcurrentQueue 中 Node* 一一对应）
        std::atomic<size_t> queued_{0};
        /// 已提交尚未执行完的任务数（在途，含正在 execute 的）
        std::atomic<size_t> pending_{0};

        // ---- worker 唤醒（队列从空到非空） ----
        std::mutex wake_mutex_;
        std::condition_variable wake_cv_;

        // ---- 队列空间等待（kBlock 策略：队列满到有空位） ----
        std::mutex space_mutex_;
        std::condition_variable space_cv_;

        // ---- 空闲通知（用于 wait_all） ----
        std::mutex idle_mutex_;
        std::condition_variable idle_cv_;

        // ---- worker 线程 ----
        std::vector<std::unique_ptr<WorkerData>> workers_;
    };

}  // namespace dobot::utils
