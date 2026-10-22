// 线程命名（Linux pthread comm，≤15 字符；htop 显示）本工程约定：
// - dobot_main：主线程**仅在** init 与 Start 完成之后、sigwait 之前设置（main.cpp）；
//   切勿在 main 最开头设名，否则子线程在自带命名前会继承该名，出现多条同名。
// - d_tp_<n>：AsyncTaskExecutor / ThreadPool worker（utils/thread_pool.hpp）
// - monitor_thr：MonitorStatusService 规则线程（business/.../monitor_status_proc.cpp）
// - spdlog_<n>：异步日志池（dobot_log/.../dobot_sink.hpp，需 __has_include(<utils/utils.hpp>)）
// - Fast DDS / 预编译 LED 等第三方线程：由库内部命名（如 dds.udp.*），ThreadSettings 无名字段

#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>
#include <thread>

#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#endif

namespace dobot::utils
{

    inline void set_current_thread_name(std::string_view name) noexcept
    {
        if (name.empty()) { return; }
        char buf[16];
        const std::size_t n = (name.size() < 15U) ? name.size() : 15U;
        std::memcpy(buf, name.data(), n);
        buf[n] = '\0';
#if defined(__linux__)
        (void)pthread_setname_np(pthread_self(), buf);
#endif
    }

    inline void set_current_thread_name(const char *name) noexcept
    {
        if (!name) { return; }
        set_current_thread_name(std::string_view(name));
    }

    /** 为已 start 的 std::thread 设置名字（可在目标线程体开头调用 set_current，或由其它线程调用本函数，Linux 下有效） */
    inline void set_thread_name(std::thread &t, std::string_view name) noexcept
    {
        if (name.empty() || !t.joinable()) { return; }
        char buf[16];
        const std::size_t n = (name.size() < 15U) ? name.size() : 15U;
        std::memcpy(buf, name.data(), n);
        buf[n] = '\0';
#if defined(__linux__)
        (void)pthread_setname_np(t.native_handle(), buf);
#endif
    }

}  // namespace dobot::utils
