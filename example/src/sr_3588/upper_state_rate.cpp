// SR_3588 的上肢状态帧率示例。一直订 rt/upper/state，直到 Ctrl+C。
// 回调里只做一次原子加一，周期打印放在主线程。
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "upper_state.hpp"
#include "upper_statePubSubTypes.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t g_stop = 0;

void OnSignal(int) {
    if (g_stop != 0) {
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
        std::raise(SIGINT);
        return;
    }
    g_stop = 1;
}

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "订阅 rt/upper/state，每秒打印这一秒收到的帧数和帧率。\n"
              << "帧率只按这一秒计算，不把前面的秒累计进去。\n"
              << "一直运行，Ctrl+C 结束。再按一次 Ctrl+C 会立刻结束。\n"
              << "DDS domain 默认 0。网卡默认全部，多网卡时加 --nic。\n";
}

}  // namespace

int main(int argc, char** argv) {
    dobot::sdk::SessionConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "缺少参数: " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            Usage(argv[0]);
            return 0;
        } else if (arg == "--on-robot") {
            config.where = dobot::sdk::RunWhere::kRobot;
        } else if (arg == "--domain") {
            config.domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            config.network_interface = need("--nic");
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    // 计数要活得比 Session 久。回调在 Shutdown 把 worker 停掉之后才不会再进来。
    std::atomic<uint64_t> frames{0};
    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto upper = session.Subscribe<dobot::msg::dds_::UpperState_PubSubType>(dobot::robot::sr_3588::kTopicUpperState);
    if (!upper.valid()) {
        std::cerr << "创建订阅失败\n";
        return 1;
    }
    upper.On([&frames](const dobot::msg::dds_::UpperState_&) { frames.fetch_add(1, std::memory_order_relaxed); });

    std::cout << "订阅 rt/upper/state。Ctrl+C 结束。\n" << std::flush;

    uint64_t last_frames = 0;
    auto window_start = std::chrono::steady_clock::now();
    while (g_stop == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto now = std::chrono::steady_clock::now();
        const double window_sec = std::chrono::duration<double>(now - window_start).count();
        if (window_sec < 1.0) {
            continue;
        }

        const uint64_t total = frames.load(std::memory_order_relaxed);
        const uint64_t window_frames = total - last_frames;
        const double window_hz = static_cast<double>(window_frames) / window_sec;
        std::cout << "matched=" << upper.Matched() << " frames=" << window_frames << " hz=" << window_hz << "\n"
                  << std::flush;
        last_frames = total;
        window_start = now;
    }

    std::cout << "已停止，累计 frames=" << frames.load(std::memory_order_relaxed) << "\n";
    return 0;
}
