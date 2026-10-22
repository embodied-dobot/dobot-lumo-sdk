#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>

namespace dobot {

template <typename T> class Singleton {
public:
  Singleton(const Singleton &) = delete;
  Singleton &operator=(const Singleton &) = delete;
  Singleton(Singleton &&) = delete;
  Singleton &operator=(Singleton &&) = delete;

  static T &GetInstance() {
    std::call_once(init_flag_, [&]() { instance_.reset(new T()); });
    return *instance_;
  }

  template <typename... Args> static T &GetInstance(Args &&...args) {
    bool expected = false;
    if (constructed_.compare_exchange_strong(expected, true)) {
      std::call_once(init_flag_, [&]() {
        try {
          instance_.reset(new T(std::forward<Args>(args)...));
        } catch (...) {
          constructed_ = false;
          throw;
        }
      });
    } else {
      throw std::runtime_error("Singleton already initialized. Cannot "
                               "reinitialize with new parameters.");
    }
    return *instance_;
  }

  static bool IsInitialized() { return instance_ != nullptr; }

protected:
  Singleton() = default;
  virtual ~Singleton() = default;

private:
  static std::unique_ptr<T> instance_;
  static std::once_flag init_flag_;
  static std::atomic<bool> constructed_;
};

template <typename T> std::unique_ptr<T> Singleton<T>::instance_{nullptr};

template <typename T> std::once_flag Singleton<T>::init_flag_;

template <typename T> std::atomic<bool> Singleton<T>::constructed_{false};

#define ENABLE_SINGLETON(ClassName) friend class Singleton<ClassName>;
} // namespace dobot