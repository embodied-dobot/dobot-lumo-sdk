#pragma once
#include "dobot_sink.hpp"
#include <memory>

namespace dobot
{
    namespace log
    {
        /**
         * 日志管理类（单例模式）
         */
        class LoggerManager : public dobot::Singleton<LoggerManager>
        {
            ENABLE_SINGLETON(LoggerManager)
        public:
            /**
             * 获取日志器
             * 共享指针指向日志器
             */
            std::shared_ptr<spdlog::logger> GetLogger();

            /**
             * 刷新日志缓冲区
             */
            void Flush();

            /**
             * 日志资源释放
             */
            void Shutdown();

            /**
             * 注册进程推出时的清理工作
             */
            void RegisterCleanup();

            ~LoggerManager();

        private:
            LoggerManager();
            LoggerManager(const LoggerManager &) = delete;
            LoggerManager &operator=(const LoggerManager &) = delete;
            LoggerManager(const LoggerManager &&) = delete;
            LoggerManager &operator=(const LoggerManager &&) = delete;
            // 根据策略创建具体的策略实现
            std::unique_ptr<LogStrategyInterface> CreateStrategy();

            std::atomic<bool> cleanup_registered_;
            std::unique_ptr<LogStrategyInterface> strategy_;
            std::shared_ptr<spdlog::logger> logger_;
        };

    }  // namespace log
}  // namespace dobot