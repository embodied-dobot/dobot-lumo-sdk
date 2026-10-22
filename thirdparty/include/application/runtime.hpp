// 异步运行时：AsyncTaskExecutor（线程池）；供 DdsClientFramework 与独立测试组合使用。
#pragma once

#include <thread/thread_pool.hpp>
#include <dobot_log.hpp>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace dobot::dds
{

    /** 基于内部 ThreadPool：post 与 ThreadPool::submit 一致，可传可调用对象及其参数，返回对应 std::future。 */
    class AsyncTaskExecutor
    {
    public:
        explicit AsyncTaskExecutor(size_t worker_count = 4, size_t max_pending = 5000) : worker_count_(worker_count), max_pending_(max_pending)
        {
            if (pool_) { return; }
            pool_ = std::make_unique<dobot::utils::ThreadPool>(worker_count_, max_pending_);
        }

        ~AsyncTaskExecutor() { stop(); }

        AsyncTaskExecutor(const AsyncTaskExecutor &) = delete;
        AsyncTaskExecutor &operator=(const AsyncTaskExecutor &) = delete;

        template <typename F, typename... Args>
        auto post(F &&f, Args &&...args) -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
        {
            static_assert(std::is_invocable_v<std::decay_t<F>, std::decay_t<Args>...>, "post(F, Args...)");
            return pool_->submit(std::forward<F>(f), std::forward<Args>(args)...);
        }

        // 无返回值的任务
        template <typename F, typename... Args>
        bool post_detached(F &&f, Args &&...args)
        {
            return pool_->submit_detached(std::forward<F>(f), std::forward<Args>(args)...);
        }

        void stop() noexcept
        {
            if (!pool_) { return; }
            pool_->shutdown();
            pool_.reset();
        }

        size_t workerCount() const noexcept { return worker_count_; }

        size_t maxPending() const noexcept { return max_pending_; }

        bool isRunning() const noexcept { return static_cast<bool>(pool_); }

    private:
        const size_t worker_count_;
        const size_t max_pending_;
        std::unique_ptr<dobot::utils::ThreadPool> pool_;
    };

}  // namespace dobot::dds
