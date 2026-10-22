#pragma once

#include "singleton.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace dobot::log
{

    enum class ConsoleType {
        STDOUT,        // 标准输出（无颜色）
        STDOUT_COLOR,  // 彩色标准输出
        STDERR,        // 标准错误输出
        STDERR_COLOR   // 彩色错误输出
    };

    enum class StrategyType {
        STRATEGY_SIZE,  // 按文件大小滚动
        STRATEGY_TIME   // 阿时间来滚动
    };

    inline std::string trim(const std::string &s)
    {
        const char *space = " \t\n\r\f\v";
        auto start = s.find_first_not_of(space);
        if (start == std::string::npos) return {};
        auto end = s.find_last_not_of(space);
        return s.substr(start, end - start + 1);
    }

    template <typename T>
    struct EnvParserTraits {
        static T parse(const char *value)
        {
            if constexpr (std::is_same_v<T, bool>) {
                if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0) { return true; }
                if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0) { return false; }
                throw std::invalid_argument("Invalid bool environment variable");
            } else if constexpr (std::is_integral_v<T>) {
                char *end = nullptr;
                errno = 0;
                if constexpr (std::is_unsigned_v<T>) {
                    unsigned long long result = std::strtoull(value, &end, 10);
                    if (errno == ERANGE || end == value || result > std::numeric_limits<T>::max()) { throw std::invalid_argument("Parse unsigned integer failed"); }
                    return static_cast<T>(result);
                } else {
                    long long result = std::strtoll(value, &end, 10);
                    if (errno == ERANGE || end == value || result < std::numeric_limits<T>::min() || result > std::numeric_limits<T>::max()) {
                        throw std::invalid_argument("Parse signed integer failed");
                    }
                    return static_cast<T>(result);
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                return std::string(value);
            } else if constexpr (std::is_same_v<T, const char *>) {
                return value;
            } else if constexpr (std::is_floating_point_v<T>) {
                char *end = nullptr;
                errno = 0;
                double result = std::strtod(value, &end);
                if (errno == ERANGE || end == value) throw std::invalid_argument("Parse floating failed");
                return static_cast<T>(result);
            } else {
                static_assert(sizeof(T) == 0, "Unsupported type for EnvParserTraits.");
            }
        }
    };

    template <>
    struct EnvParserTraits<StrategyType> {
        static StrategyType parse(const char *value)
        {
            static const std::unordered_map<std::string, StrategyType> name_map = {{"DOBOT_STRATEGY_SIZE", StrategyType::STRATEGY_SIZE}, {"DOBOT_STRATEGY_TIME", StrategyType::STRATEGY_TIME}};

            if (value == nullptr) throw std::invalid_argument("Environment variable value is null");

            std::string key = trim(value);
            if (key.empty()) throw std::invalid_argument("Empty StrategyType string");
            auto it = name_map.find(key);
            if (it == name_map.end()) throw std::invalid_argument("Unknown StrategyType name: " + key);
            return it->second;
        }
    };

    template <>
    struct EnvParserTraits<ConsoleType> {
        static ConsoleType parse(const char *value)
        {
            static const std::unordered_map<std::string, ConsoleType> name_map = {{"DOBOT_CONSOLE_STDOUT", ConsoleType::STDOUT},
                                                                                  {"DOBOT_CONSOLE_COLOR", ConsoleType::STDOUT_COLOR},
                                                                                  {"DOBOT_CONSOLE_STDERR", ConsoleType::STDERR},
                                                                                  {"DOBOT_CONSOLE_STDERR_COLOR", ConsoleType::STDERR_COLOR}};

            if (value == nullptr) throw std::invalid_argument("Environment variable value is null");

            std::string key = trim(value);
            if (key.empty()) throw std::invalid_argument("Empty ConsoleType string");
            auto it = name_map.find(key);
            if (it == name_map.end()) throw std::invalid_argument("Unknown ConsoleType name: " + key);
            return it->second;
        }
    };

    template <typename T>
    struct EnvCache {
        static std::unordered_map<std::string, T> cache;
    };

    template <typename T>
    std::unordered_map<std::string, T> EnvCache<T>::cache;

    class EnvParser : public dobot::Singleton<EnvParser>
    {
        ENABLE_SINGLETON(EnvParser)
    public:
        template <typename T>
        T get(const std::string &name, T default_val = T())
        {
            auto &cache = EnvCache<T>::cache;

            std::lock_guard<std::mutex> lock(mutex_);

            auto it = cache.find(name);
            if (it != cache.end()) { return it->second; }

            const char *env_val = std::getenv(name.c_str());

            if (!env_val) {
                cache[name] = default_val;
                return default_val;
            }

            try {
                T parsed_val = EnvParserTraits<T>::parse(env_val);
                cache[name] = parsed_val;
                return parsed_val;
            } catch (const std::exception &e) {
                cache[name] = default_val;
                return default_val;
            }
        }

    private:
        EnvParser() = default;
        std::mutex mutex_;
    };

}  // namespace dobot::log