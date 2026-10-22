#pragma once
/**
 * @file dobot_logger.h
 * @brief 日志系统封装
 * @version 1.0.0
 * @date 2026.4.9
 *
 * 使用说明：
 * 1. 在程序入口调用 DOBOT_INIT() 初始化日志系统
 * 2. 使用 DOBOT_TRACE(),DOBOT_DEBUG(), DOBOT_INFO(), DOBOT_WARN(),
 * DOBOT_ERROR(), DOBOT_CRITICAL() 打印日志
 * 3. 日志系统会在程序退出时自动清理资源
 *
 * 环境变量配置：
 * - DOBOT_FILE_LOG_MAX_SIZE: 单个文件最大大小(字节)
 * - DOBOT_FILE_LOG_MAX_NUM: 最大文件数
 * - DOBOT_FILE_LOG_QUE_SIZE: 异步队列大小 默认 8192
 * - DOBOT_FILE_LOG_THREAD_SIZE: 异步线程数 默认 1
 * - DOBOT_LOG_PATTERN:             日志格式  默认 ‘[%Y-%m-%d
%H:%M:%S.%e][id:%t][%^%l%$][%s:%#] %v’
 * - DOBOT_FILE_LOG_PATH: 日志文件目录
 * - DOBOT_FILE_LOG_SAVE_ENABLE    日志是否落盘存文件 (true/false)
 * - DOBOT_CONSOLE_ENABLE          终端是否打印日志 (true/false)
 * - DOBOT_FILE_LOG_ASYNC_ENABLE   flie日志是否开启异步 (true/false)
 * - DOBOT_FILE_LOG_ROTATE_ENABLE  日志存盘是否开始滚动策略 (true/false)
 * - DOBOT_FILE_LOG_LEVEL          存盘日志等级
(trace,debug,info,warn,error,critical)
 * - DOBOT_CONSOLE_LEVEL           终端打印日志等级
(trace,debug,info,warn,error,critical)
 * - DOBOT_CONSOLE_STDOUT          终端打印日志显示颜色策略
(DOBOT_CONSOLE_STDOUT,DOBOT_CONSOLE_COLOR, DOBOT_CONSOLE_STDERR,
DOBOT_CONSOLE_STDERR_COLOR)
 * - DOBOT_STRATEGY_TYPE
日志滚动策略 1.按单个日志文件大小滚动(DOBOT_STRATEGY_SIZE)，2.按时间交割滚动(DOBOT_STRATEGY_TIME)
 */
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <atomic>
#include <chrono>
#include <cstdint>
#include "dobot_logger_manager.hpp"

namespace dobot
{
    namespace log
    {
        namespace detail
        {

            inline std::uint64_t dobot_log_now_ms()
            {
                return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            }

            template <typename Fn>
            inline void dobot_log_rate_limit(std::atomic<std::uint64_t> &last_ms, std::uint64_t interval_ms, Fn &&fn)
            {
                if (interval_ms == 0) {
                    fn();
                    return;
                }

                const std::uint64_t now_ms = dobot_log_now_ms();
                std::uint64_t previous_ms = last_ms.load(std::memory_order_relaxed);

                while (previous_ms == 0 || now_ms - previous_ms >= interval_ms) {
                    if (last_ms.compare_exchange_weak(previous_ms, now_ms, std::memory_order_relaxed, std::memory_order_relaxed)) {
                        fn();
                        return;
                    }
                }
            }

        }  // namespace detail
    }  // namespace log
}  // namespace dobot

// ============================================
// 用户接口宏定义
// ============================================

/**
 * @brief 初始化日志系统宏
 * @note 必须在其他日志宏之前调用
 */
#define DOBOT_INIT()                                                                                                                                                                                   \
    do {                                                                                                                                                                                               \
        static bool __dobot_log_initialized__ = []() -> bool {                                                                                                                                         \
            auto &manager = dobot::log::LoggerManager::GetInstance();                                                                                                                                  \
            spdlog::flush_every(std::chrono::seconds(3));                                                                                                                                              \
            return true;                                                                                                                                                                               \
        }();                                                                                                                                                                                           \
        (void)__dobot_log_initialized__;                                                                                                                                                               \
    } while (0)

/**
 * @brief 带格式的日志宏
 */
#define DOBOT_LOG(level, ...)                                                                                                                                                                          \
    do {                                                                                                                                                                                               \
        /* DOBOT_INIT();*/                                                                                                                                                                             \
        auto logger = dobot::log::LoggerManager::GetInstance().GetLogger();                                                                                                                            \
        if (logger) { SPDLOG_LOGGER_##level(logger, __VA_ARGS__); }                                                                                                                                    \
    } while (0)

// ==================== 便捷级别宏（内部转为大写） ====================
#define DOBOT_TRACE(...) DOBOT_LOG(TRACE, __VA_ARGS__)
#define DOBOT_DEBUG(...) DOBOT_LOG(DEBUG, __VA_ARGS__)
#define DOBOT_INFO(...) DOBOT_LOG(INFO, __VA_ARGS__)
#define DOBOT_WARN(...) DOBOT_LOG(WARN, __VA_ARGS__)
#define DOBOT_ERROR(...) DOBOT_LOG(ERROR, __VA_ARGS__)
#define DOBOT_CRITICAL(...) DOBOT_LOG(CRITICAL, __VA_ARGS__)
/**
 * @brief 条件日志宏
 */
#define DOBOT_DEBUG_IF(cond, ...)                                                                                                                                                                      \
    do {                                                                                                                                                                                               \
        if (cond) { DOBOT_DEBUG(__VA_ARGS__); }                                                                                                                                                        \
    } while (0)

#define DOBOT_INFO_IF(cond, ...)                                                                                                                                                                       \
    do {                                                                                                                                                                                               \
        if (cond) { DOBOT_INFO(__VA_ARGS__); }                                                                                                                                                         \
    } while (0)

/**
 * @brief 一次性日志宏（避免重复初始化检查）
 */
#define DOBOT_LOG_ONCE(level, ...)                                                                                                                                                                     \
    do {                                                                                                                                                                                               \
        static std::once_flag __flag__;                                                                                                                                                                \
        std::call_once(__flag__, [&]() { DOBOT_LOG(level, __VA_ARGS__); });                                                                                                                            \
    } while (0)

/**
 * @brief 带频率限制的日志宏（无锁，同一调用点至少间隔 interval_ms 毫秒打印一次）
 */
#define DOBOT_LOG_RATE_LIMITED(level, interval_ms, ...)                                                                                                                                                \
    do {                                                                                                                                                                                               \
        static std::atomic<std::uint64_t> __dobot_log_last_ms{0};                                                                                                                                      \
        dobot::log::detail::dobot_log_rate_limit(__dobot_log_last_ms, static_cast<std::uint64_t>(interval_ms), [&]() { DOBOT_LOG(level, __VA_ARGS__); });                                              \
    } while (0)

/**
 * @brief 带作用域的日志宏
 */
class DobotLogScope
{
public:
    DobotLogScope(const char *function, const char *file, int line) : function_(function), file_(file), line_(line) { DOBOT_DEBUG("[ENTER] {} ({}:{})", function_, file_, line_); }

    ~DobotLogScope() { DOBOT_DEBUG("[EXIT] {} ({}:{})", function_, file_, line_); }

private:
    const char *function_;
    const char *file_;
    int line_;
};

#define DOBOT_SCOPE_LOG() DobotLogScope __dobot_log_scope__(__FUNCTION__, __FILE__, __LINE__)

/**
 * @brief 高并法场景下异步刷新策略可以调用这个接口
 * @note 一次就可以，在调用DOBOT_INIT()
 * 后调用一次接口即可 每隔 second 秒 自动flush落盘刷新
 */
#define DOBOT_LOG_FLUSH_EVERY_TIME(second) spdlog::flush_every(std::chrono::seconds(second));
// ===========================================
// 编译时开关
// ===========================================

// 编译时禁用特定级别的日志
#ifdef DOBOT_DISABLE_TRACE_LOG
#undef DOBOT_TRACE
#define DOBOT_TRACE(...) ((void)0)
#endif

#ifdef DOBOT_DISABLE_DEBUG_LOG
#undef DOBOT_DEBUG
#define DOBOT_DEBUG(...) ((void)0)
#endif

#ifdef DOBOT_DISABLE_INFO_LOG
#undef DOBOT_INFO
#define DOBOT_INFO(...) ((void)0)
#endif

#ifdef DOBOT_DISABLE_ARN_LOG
#undef DOBOT_WARN
#define DOBOT_WARN(...) ((void)0)
#endif

#ifdef DOBOT_DISABLE_ERROR_LOG
#undef DOBOT_ERROR
#define DOBOT_ERROR(...) ((void)0)
#endif

// 编译时禁用所有日志
#ifdef DOBOT_DISABLE_ALL_LOGS
#undef DOBOT_TRACE
#define DOBOT_TRACE(...) ((void)0)
#undef DOBOT_DEBUG
#define DOBOT_DEBUG(...) ((void)0)
#define DOBOT_INFO(...) ((void)0)
#define DOBOT_WARN(...) ((void)0)
#define DOBOT_ERROR(...) ((void)0)
#define DOBOT_CRITICAL(...) ((void)0)
#endif
