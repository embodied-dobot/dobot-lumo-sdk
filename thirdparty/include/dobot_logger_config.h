#pragma once

#include "dobot_env_loader.hpp"
#include "singleton.hpp"
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

constexpr const char *file_log_path_env = "DOBOT_FILE_LOG_PATH";
constexpr const char *file_log_max_size_env = "DOBOT_FILE_LOG_MAX_SIZE";
constexpr const char *file_log_max_num_env = "DOBOT_FILE_LOG_MAX_NUM";
constexpr const char *file_log_async_enable_env = "DOBOT_FILE_LOG_ASYNC_ENABLE";
constexpr const char *file_log_rotate_enable_env = "DOBOT_FILE_LOG_ROTATE_ENABLE";
constexpr const char *file_log_save_enable_env = "DOBOT_FILE_LOG_SAVE_ENABLE";
constexpr const char *file_log_que_size_env = "DOBOT_FILE_LOG_QUE_SIZE";
constexpr const char *file_log_thread_size_env = "DOBOT_FILE_LOG_THREAD_SIZE";
constexpr const char *file_log_level_env = "DOBOT_FILE_LOG_LEVEL";  // trace,debug,info,warn,error,critical,off
constexpr const char *console_level_env = "DOBOT_CONSOLE_LEVEL";    // trace,debug,info,warn,error,critical,off
constexpr const char *console_enable_env = "DOBOT_CONSOLE_ENABLE";  // true,false
constexpr const char *file_log_pattern_env = "DOBOT_LOG_PATTERN";

constexpr const char *file_log_pattern_default = "[%Y-%m-%d %H:%M:%S.%e][%t][%^%l%$][%s:%#] %v";
constexpr const char *console_stdout_env = "DOBOT_CONSOLE_STDOUT";  // DOBOT_CONSOLE_STDOUT,
                                                                    // DOBOT_CONSOLE_COLOR,DOBOT_CONSOLE_STDERR,DOBOT_CONSOLE_STDERR_COLOR
constexpr const char *strategy_type_evn = "DOBOT_STRATEGY_TYPE";    // DOBOT_STRATEGY_SIZE, DOBOT_STRATEGY_TIME

namespace dobot
{
    namespace log
    {
        /**
         * 获取当前进程的可执行文件名（不含路径）
         * @return 进程名字字符串
         */
        inline std::string get_process_name()
        {
            char buffer[PATH_MAX];

#ifdef __linux__
            ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
            if (len == -1) { throw std::runtime_error("Failed to read /proc/self/exe"); }
            buffer[len] = '\0';

#elif defined(__APPLE__)
            uint32_t size = PATH_MAX;
            if (_NSGetExecutablePath(buffer, &size) != 0) { throw std::runtime_error("Failed to get executable path"); }

#else
            // 其他 Unix 系统
            if (readlink("/proc/curproc/file", buffer, sizeof(buffer) - 1) == -1) {
                // 备用方案：使用 argv[0]
                extern char **environ;
                char **argv = environ - 1;
                while (argv > environ && *(argv - 1)) throw std::runtime_error("Platform not supported for getting process name");
            }
#endif

            // 使用 basename 提取文件名部分
            char *base = basename(buffer);
            if (base == nullptr) { throw std::runtime_error("Failed to extract basename"); }

            // 创建副本以避免修改原始数据
            std::string result(base);

            // 移除扩展名（如果是 .exe 或其他）
            size_t dot_pos = result.find_last_of('.');
            if (dot_pos != std::string::npos) { result = result.substr(0, dot_pos); }

            return result;
        }

        class LoggerConfig : public dobot::Singleton<LoggerConfig>
        {
            enum class LEVEL_INFO { FILE_LEVEL, CONSOLE_LEVEL };
            ENABLE_SINGLETON(LoggerConfig)
        public:
            void LoadFromEnv()
            {
                log_path_ = dobot::log::EnvParser::GetInstance().get(file_log_path_env, "./logs");
                log_file_name_ = get_process_name() + ".log";
                log_file_max_size_ = dobot::log::EnvParser::GetInstance().get(file_log_max_size_env, 10 * 1024 * 1024);
                log_file_max_num_ = dobot::log::EnvParser::GetInstance().get(file_log_max_num_env, 5);
                log_file_level_ = GetLogLevel(LEVEL_INFO::FILE_LEVEL);
                log_async_enabled_ = dobot::log::EnvParser::GetInstance().get(file_log_async_enable_env, true);
                log_file_save_enabled_ = dobot::log::EnvParser::GetInstance().get(file_log_save_enable_env, true);
                log_file_rotate_enabled_ = dobot::log::EnvParser::GetInstance().get(file_log_rotate_enable_env, true);
                file_log_queue_size_ = dobot::log::EnvParser::GetInstance().get(file_log_que_size_env, 8192);
                file_log_thread_num_ = dobot::log::EnvParser::GetInstance().get(file_log_thread_size_env, 1);
                console_level_ = GetLogLevel(LEVEL_INFO::CONSOLE_LEVEL);
                console_enabled_ = dobot::log::EnvParser::GetInstance().get(console_enable_env, false);
                file_pattern_ = dobot::log::EnvParser::GetInstance().get(file_log_pattern_env, file_log_pattern_default);
                console_type_ = dobot::log::EnvParser::GetInstance().get(console_stdout_env, ConsoleType::STDERR_COLOR);
                strategy_type_ = dobot::log::EnvParser::GetInstance().get(strategy_type_evn, StrategyType::STRATEGY_SIZE);
            }

            std::string log_path() { return log_path_; }
            std::string log_file_name() { return log_file_name_; }
            size_t log_file_max_size() { return log_file_max_size_; }
            size_t log_file_max_num() { return log_file_max_num_; }
            spdlog::level::level_enum log_file_level() { return log_file_level_; }
            size_t file_log_queue_size() { return file_log_queue_size_; }
            size_t file_log_thread_num() { return file_log_thread_num_; }
            bool log_async_enabled() { return log_async_enabled_; }
            bool log_file_save_enable() { return log_file_save_enabled_; }
            spdlog::level::level_enum console_level() { return console_level_; }
            bool console_enable() { return console_enabled_; }
            bool log_file_rotate_enable() { return log_file_rotate_enabled_; }
            std::string file_pattern() { return file_pattern_; }
            ConsoleType console_stdout_type() { return console_type_; }
            StrategyType strategy_type() { return strategy_type_; }

        private:
            LoggerConfig() {}

            spdlog::level::level_enum GetLogLevel(LEVEL_INFO level_info)
            {
                static const std::unordered_map<std::string, spdlog::level::level_enum> level_map = {
                    {"trace", spdlog::level::trace}, {"debug", spdlog::level::debug},       {"info", spdlog::level::info}, {"warn", spdlog::level::warn},
                    {"error", spdlog::level::err},   {"critical", spdlog::level::critical}, {"off", spdlog::level::off}};
                std::string level;
                if (level_info == LEVEL_INFO::FILE_LEVEL) {
                    level = dobot::log::EnvParser::GetInstance().get(file_log_level_env, "error");
                } else if (level_info == LEVEL_INFO::CONSOLE_LEVEL) {
                    level = dobot::log::EnvParser::GetInstance().get(console_level_env, "error");
                }

                auto it = level_map.find(level);
                if (it != level_map.end()) { return it->second; }

                std::cout << "----------not find levle info:" << level << "----------" << std::endl;
                return spdlog::level::err;
            }

            std::string log_path_{"./logs"};
            std::string log_file_name_{"app.log"};
            size_t log_file_max_size_{10 * 1024 * 1024};                    // 默认一个log文件大小10M
            size_t log_file_max_num_{5};                                    // 滚动文件个数
            spdlog::level::level_enum log_file_level_{spdlog::level::err};  // Log文件落盘的日志等级
            size_t file_log_queue_size_{8192};                              // queue缓冲区大小
            size_t file_log_thread_num_{1};                                 // 异步线程池大小
            bool log_async_enabled_{true};                                  // 日志是否异步
            bool log_file_save_enabled_{true};
            bool log_file_rotate_enabled_{true};  // 文件滚动是否开启

            spdlog::level::level_enum console_level_{spdlog::level::err};  // 终端打印的日志等级
            bool console_enabled_{false};                                  // 是否开启终端打印
            std::string file_pattern_{file_log_pattern_default};           // 输出格式
            ConsoleType console_type_{ConsoleType::STDERR_COLOR};          // 终端输出颜色
            StrategyType strategy_type_{StrategyType::STRATEGY_SIZE};      // 日志滚动策略
        };
    }  // namespace log
}  // namespace dobot
