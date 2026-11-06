// SR_thor 的事件订阅示例。急停不是周期控制量，来一帧就进 On()。
// 关节状态不要这样写，用 dds_echo 里的 Latest()。
#include "dobot/robot/sr_thor/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "mcu_data.hpp"
#include "mcu_dataPubSubTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " [--seconds 5] [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "订阅 Mcu/Estop。收到一帧就在回调里打印。0 是急停，1 是正常。\n"
              << "DDS domain 默认 0。网卡默认全部，多网卡时加 --nic。\n"
              << "回调里不要再调用 RPC 或 Shutdown。\n";
}

}  // namespace

int main(int argc, char** argv) {
    dobot::sdk::SessionConfig config;
    int seconds = 5;
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
        } else if (arg == "--seconds") {
            seconds = std::stoi(need("--seconds"));
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thor::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto estop = session.Subscribe<dobot::msg::dds_::Estop_PubSubType>(dobot::robot::sr_thor::kTopicMcuEstop);
    if (!estop.valid()) {
        std::cerr << "创建订阅失败\n";
        return 1;
    }

    std::atomic<int> callbacks{0};
    estop.On([&callbacks](const dobot::msg::dds_::Estop_& msg) {
        const int n = ++callbacks;
        std::cout << "回调 #" << n << " stop1=" << static_cast<unsigned>(msg.stop1())
                  << " stop2=" << static_cast<unsigned>(msg.stop2()) << "\n";
    });

    std::cout << "等待 Mcu/Estop 回调，最长 " << seconds << " 秒\n";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline && callbacks.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "callbacks=" << callbacks.load() << " matched=" << estop.Matched() << "\n";
    if (auto latest = estop.Latest()) {
        std::cout << "Latest stop1=" << static_cast<unsigned>(latest->stop1())
                  << " stop2=" << static_cast<unsigned>(latest->stop2()) << "\n";
    }
    if (callbacks.load() == 0) {
        std::cerr << "没有收到急停话题。请确认 --domain 与机器人一致。多网卡时用 --nic 指定接机器人的网卡。\n";
        return 1;
    }
    return 0;
}
