#pragma once
#include "dobot_logger_config.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/hourly_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h> // stdout_color_sink_mt, stdout_sink_mt
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#include <string>

namespace dobot {
namespace log {
namespace fs = std::filesystem;

/**
 * 日志输出配置
 */
struct LogOutputConfig {
  bool enable_console = true;
  bool enable_file = true;
  std::string file_path = "./logs";
  std::string file_name = "app.log";
  bool async_mode = false;
  size_t queue_size = 8192; // 异步队列大小
  size_t thread_count = 1;  // 异步线程数

  spdlog::level::level_enum file_level = spdlog::level::debug;
  std::string pattern;
  spdlog::level::level_enum flush_on_level = spdlog::level::err;
  spdlog::level::level_enum console_level = spdlog::level::debug;

  ConsoleType console_type = ConsoleType::STDOUT_COLOR;
};

/**
 * 日志滚动配置
 */
struct LogRotateConfig {
  bool enable_rotation = true;

  // 大小滚动配置
  size_t max_file_size = 10 * 1024 * 1024; // 10MB
  size_t max_files = 5;                    // 保留文件数
  bool rotate_on_open = true;

  // 时间滚动配置
  int rotation_hour = 0;   // 每日滚动时间（小时）
  int rotation_minute = 0; // 每日滚动时间（分钟）
  size_t max_days = 0;     // 保留天数（0表示无限制）
};

/**
 * 日志滚动策略接口
 * 用于接入端暴露在外部的接口
 */
class LogStrategyInterface {
public:
  virtual ~LogStrategyInterface() = default;
  virtual std::shared_ptr<spdlog::logger> CreateLogger() = 0;
  virtual std::shared_ptr<spdlog::logger> GetLogger() const = 0;
  virtual std::string GetStrategyName() const = 0;
};

/**
 * 日志滚动策略基类
 * Derived 具体策略实现类
 */
template <typename Derived>
class LogRotateStrategyBase : public LogStrategyInterface {
protected:
  std::string logger_name_;
  std::string file_path_;
  LogRotateConfig rotate_config_;
  std::shared_ptr<spdlog::logger> logger_;
  bool async_mode_ = false;

public:
  LogRotateStrategyBase() {
    LoggerConfig::GetInstance().LoadFromEnv();
    logger_name_ = LoggerConfig::GetInstance().log_file_name();
    file_path_ = LoggerConfig::GetInstance().log_path();
    async_mode_ = LoggerConfig::GetInstance().log_async_enabled();
    rotate_config_.enable_rotation =
        LoggerConfig::GetInstance().log_file_rotate_enable();
    rotate_config_.max_file_size =
        LoggerConfig::GetInstance().log_file_max_size();
    rotate_config_.max_files = LoggerConfig::GetInstance().log_file_max_num();

    CreateDir(file_path_);
  }

  virtual ~LogRotateStrategyBase() = default;

  /**
   * 创建日志器（纯虚函数）
   */
  virtual std::shared_ptr<spdlog::logger> CreateLogger() = 0;

  /**
   * 获取日志器
   */
  std::shared_ptr<spdlog::logger> GetLogger() const { return logger_; }

  /**
   * 检查滚动是否启用
   */
  bool IsRoataionEnabled() const { return rotate_config_.enable_rotation; }

  /**
   * 启用/禁用滚动
   */
  void EnableRotation(bool enable) {
    if (rotate_config_.enable_rotation != enable) {
      rotate_config_.enable_rotation = enable;

      // 重新创建日志器以应用配置变更
      if (logger_) {
        spdlog::drop(logger_->name());
        logger_ = CreateLogger();
      }
    }
  }

  /**
   * 更新滚动配置
   */
  void UpdateRotateConfig(const LogRotateConfig &config) {
    rotate_config_ = config;

    // 重新创建日志器以应用新配置
    if (logger_) {
      auto old_name = logger_->name();
      spdlog::drop(old_name);
      logger_ = CreateLogger();

      if (logger_) {
        printf("Rotation config updated - MaxSize: %ld bytes, MaxFiles: %ld, "
               "RotateTime: {:%02d}:{:%02d}, MaxDays: %ld\n",
               rotate_config_.max_file_size, rotate_config_.max_files,
               rotate_config_.rotation_hour, rotate_config_.rotation_minute,
               rotate_config_.max_days);
      }
    }
  }

  /**
   * 获取策略名称
   */
  virtual std::string GetStrategyName() const = 0;

protected:
  /**
   * 验证文件路径
   */

  bool CreateDir(const std::string &dir_path, bool create_parents = true) {
    try {
      if (dir_path.empty()) {
        std::cerr << "Directory path is empty" << std::endl;
        return false;
      }

      fs::path path(dir_path);

      if (fs::exists(path)) {
        if (fs::is_directory(path)) {
          return true; // 目录已存在
        } else {
          std::cerr << "Path exists but is not a directory: " << dir_path
                    << std::endl;
          return false; // 路径存在但不是目录
        }
      }

      bool success;
      if (create_parents) {
        success = fs::create_directories(path); // 创建所有必要的父目录
      } else {
        success = fs::create_directory(path); // 只创建最后一级目录
      }

      if (success) {
        std::cout << "Successfully created directory: " << dir_path
                  << std::endl;
        return true;
      } else {
        std::cerr << "Failed to create directory: " << dir_path << std::endl;
        return false;
      }
    } catch (const fs::filesystem_error &e) {
      std::cerr << "Filesystem error creating directory '" << dir_path
                << "': " << e.what() << std::endl;
      return false;
    } catch (const std::exception &e) {
      std::cerr << "Error creating directory '" << dir_path << "': " << e.what()
                << std::endl;
      return false;
    }
  }

  bool ValidFilePath() {
    try {
      fs::path path(file_path_);
      return fs::exists(path) && fs::is_directory(path);
    } catch (const fs::filesystem_error &e) {
      std::cerr << "Filesystem error checking directory '" << file_path_
                << "': " << e.what() << std::endl;
      return false;
    }
  }

  /**
   * 配置日志器公共属性
   */
  void ConfigureLogger() {
    if (!logger_)
      return;

    // 设置日志级别（默认为trace）
    logger_->set_level(spdlog::level::trace);

    // 设置日志格式
    logger_->set_pattern(LoggerConfig::GetInstance().file_pattern());

    // 设置错误级别立即刷新
    logger_->flush_on(spdlog::level::err);

    // 注册日志器
    spdlog::register_logger(logger_);
  }
};

/**
 * 支持多目标的滚动策略基类（console+file）
 * Derived 具体策略实现类
 */
template <typename Derived>
class MultiRotateStrategy : public LogRotateStrategyBase<Derived> {
protected:
  LogOutputConfig output_config_;
  std::shared_ptr<spdlog::sinks::sink> console_sink_;
  std::shared_ptr<spdlog::sinks::sink> file_sink_;

  using Base = LogRotateStrategyBase<Derived>;

public:
  MultiRotateStrategy() {
    output_config_.async_mode = LoggerConfig::GetInstance().log_async_enabled();
    output_config_.console_level = LoggerConfig::GetInstance().console_level();
    output_config_.console_type = ConsoleType::STDOUT_COLOR;
    output_config_.enable_console =
        LoggerConfig::GetInstance().console_enable();
    output_config_.enable_file =
        LoggerConfig::GetInstance().log_file_save_enable();
    output_config_.file_level = LoggerConfig::GetInstance().log_file_level();
    output_config_.file_name = LoggerConfig::GetInstance().log_file_name();
    output_config_.file_path = LoggerConfig::GetInstance().log_path();
    output_config_.pattern = LoggerConfig::GetInstance().file_pattern();
    output_config_.queue_size =
        LoggerConfig::GetInstance().file_log_queue_size();
    output_config_.thread_count =
        LoggerConfig::GetInstance().file_log_thread_num();
    output_config_.console_type =
        LoggerConfig::GetInstance().console_stdout_type();
    Base::async_mode_ = output_config_.async_mode;
  }

  virtual ~MultiRotateStrategy() = default;

  std::shared_ptr<spdlog::logger> CreateLogger() override {
    std::vector<spdlog::sink_ptr> sinks;

    // 创建主滚动文件接收器
    if (output_config_.enable_file) {
      auto main_sink = CreateFileSink();
      if (main_sink) {
        main_sink->set_level(output_config_.file_level);
        sinks.emplace_back(main_sink);
      }
    }

    // 可选：创建控制台接收器
    if (output_config_.enable_console) {
      auto console_sink = CreateConsoleSink();
      if (console_sink) {
        console_sink->set_level(output_config_.console_level);
        sinks.emplace_back(console_sink);
      }
    }

    // 必须保证 sinks 至少有一个有效的 sink！
    if (sinks.empty()) {
      // 兜底策略：如果环境变量啥都没配，给一个默认的控制台输出，防止空指针崩溃
      std::cout << "-------err: sink size is 0 ,default add console------"
                << std::endl;
      sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    // 根据配置决定创建同步还是异步日志器
    if (output_config_.async_mode) {
      static std::once_flag pool_init_flag;
      std::call_once(pool_init_flag, [&]() {
        spdlog::init_thread_pool(output_config_.queue_size,
                                 output_config_.thread_count);
      });

      Base::logger_ = std::make_shared<spdlog::async_logger>(
          Base::logger_name_, sinks.begin(), sinks.end(), spdlog::thread_pool(),
          spdlog::async_overflow_policy::block);

    } else {
      // 同步日志器：直接创建
      Base::logger_ = std::make_shared<spdlog::logger>(
          Base::logger_name_, sinks.begin(), sinks.end());
    }

    Base::ConfigureLogger();

    return Base::logger_;
  }

protected:
  std::shared_ptr<spdlog::sinks::sink> CreateConsoleSink() {
    try {
      switch (output_config_.console_type) {
      case ConsoleType::STDOUT:
        return std::make_shared<spdlog::sinks::stdout_sink_mt>();
      case ConsoleType::STDOUT_COLOR:
        return std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      case ConsoleType::STDERR:
        return std::make_shared<spdlog::sinks::stderr_sink_mt>();
      case ConsoleType::STDERR_COLOR:
        return std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
      default:
        return std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      }
    } catch (const spdlog::spdlog_ex &ex) {
      std::cout << "Failed to create console sink: " << ex.what() << std::endl;
      return nullptr;
    }
  }

  virtual std::shared_ptr<spdlog::sinks::sink> CreateFileSink() = 0;
};

// 支持多目标的大小滚动策略
class MultiTargetSizeRotateStrategy
    : public MultiRotateStrategy<MultiTargetSizeRotateStrategy> {
public:
  // 定义接收器类型供异步创建使用
  using SinkType = spdlog::sinks::rotating_file_sink_mt;

private:
  using BaseMulti = MultiRotateStrategy<MultiTargetSizeRotateStrategy>;
  using BaseRotate = LogRotateStrategyBase<MultiTargetSizeRotateStrategy>;

public:
  MultiTargetSizeRotateStrategy() {
    // 验证滚动配置
    if (BaseRotate::rotate_config_.enable_rotation &&
        BaseRotate::rotate_config_.max_file_size == 0) {
      std::cout << "Rotation enabled but max_file_size is 0, using default 10MB"
                << std::endl;
      BaseRotate::rotate_config_.max_file_size = 10 * 1024 * 1024;
    }
  }

  // 创建文件接收器实现
  std::shared_ptr<spdlog::sinks::sink> CreateFileSink() override {
    try {
      std::filesystem::path full_path =
          std::filesystem::path(file_path_) / logger_name_;
      if (BaseRotate::rotate_config_.enable_rotation) {
        // 2. 构建完整日志文件路径
        return std::make_shared<SinkType>(
            full_path, BaseRotate::rotate_config_.max_file_size,
            BaseRotate::rotate_config_.max_files,
            BaseRotate::rotate_config_.rotate_on_open);
      } else {
        return std::make_shared<spdlog::sinks::basic_file_sink_mt>(full_path);
      }
    } catch (const spdlog::spdlog_ex &ex) {
      std::cout << "Failed to create file sink for " << BaseRotate::logger_name_
                << ":" << ex.what() << std::endl;
      return nullptr;
    }
  }

  std::string GetStrategyName() const override { return "SizeRotation"; }
};

// 支持多目标的时间滚动策略
class MultiTargetTimeRotateStrategy
    : public MultiRotateStrategy<MultiTargetTimeRotateStrategy> {
public:
  // 定义接收器类型供异步创建使用
  using SinkType = spdlog::sinks::daily_file_sink_mt;

private:
  using BaseMulti = MultiRotateStrategy<MultiTargetTimeRotateStrategy>;
  using BaseRotate = LogRotateStrategyBase<MultiTargetTimeRotateStrategy>;

public:
  MultiTargetTimeRotateStrategy() {
    // 验证时间配置
    if (BaseRotate::rotate_config_.enable_rotation) {
      if (BaseRotate::rotate_config_.rotation_hour < 0 ||
          BaseRotate::rotate_config_.rotation_hour > 23) {
        spdlog::warn("Invalid rotation hour {}, using default 0",
                     BaseRotate::rotate_config_.rotation_hour);
        BaseRotate::rotate_config_.rotation_hour = 0;
      }
      if (BaseRotate::rotate_config_.rotation_minute < 0 ||
          BaseRotate::rotate_config_.rotation_minute > 59) {
        spdlog::warn("Invalid rotation minute {}, using default 0",
                     BaseRotate::rotate_config_.rotation_minute);
        BaseRotate::rotate_config_.rotation_minute = 0;
      }
    }
  }

  // 创建文件接收器实现（每日滚动）
  std::shared_ptr<spdlog::sinks::sink> CreateFileSink() override {
    try {
      if (BaseRotate::rotate_config_.enable_rotation) {
        // 创建每日滚动文件接收器 [ref_1]
        return std::make_shared<SinkType>(
            BaseRotate::file_path_, BaseRotate::rotate_config_.rotation_hour,
            BaseRotate::rotate_config_.rotation_minute);
      } else {
        // 创建普通文件接收器
        return std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            BaseRotate::file_path_);
      }
    } catch (const spdlog::spdlog_ex &ex) {
      std::cout << "Failed to create daily file sink for "
                << BaseRotate::logger_name_ << ":" << ex.what() << std::endl;
      return nullptr;
    }
  }

  std::string GetStrategyName() const override { return "TimeRotation"; }

  // 设置滚动时间
  bool SetRotationTime(int hour, int minute) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
      std::cout << "Invalid rotation time: " << hour << ":" << minute
                << std::endl;
      return false;
    }

    if (!BaseRotate::rotate_config_.enable_rotation) {
      std::cout << "Rotation not enabled for logger "
                << BaseRotate::logger_name_ << std::endl;
      return false;
    }

    if (BaseRotate::rotate_config_.rotation_hour == hour &&
        BaseRotate::rotate_config_.rotation_minute == minute) {
      return true;
    }

    BaseRotate::rotate_config_.rotation_hour = hour;
    BaseRotate::rotate_config_.rotation_minute = minute;

    // 更新配置
    auto config = BaseRotate::rotate_config_;
    config.rotation_hour = hour;
    config.rotation_minute = minute;
    BaseRotate::UpdateRotateConfig(config);

    return true;
  }

  // 获取滚动时间
  std::pair<int, int> GetRotationTime() const {
    return {BaseRotate::rotate_config_.rotation_hour,
            BaseRotate::rotate_config_.rotation_minute};
  }
};

} // namespace log
} // namespace dobot
