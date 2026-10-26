// SR_3588：订阅 rt/set/fsm/id，只在技能开始和结束时打印。
// 这个话题由运控算法发布。current_action 为空或 "\0" 表示不可播放，
// "enable" 表示可播放但没在播，其它非空字符串是正在播放的技能名。
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "set_fsm_id.hpp"
#include "set_fsm_idPubSubTypes.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t g_stop = 0;

void OnSignal(int) { g_stop = 1; }

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " [--all] [--seconds 0] [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "订阅 rt/set/fsm/id。技能名出现时打印开始，回到 enable 或空时打印结束。\n"
              << "--all 时每一帧都打印，状态没变也打印。\n"
              << "--seconds 0 表示一直运行，Ctrl+C 结束。回调里不要再调用 RPC 或 Shutdown。\n";
}

bool IsIdle(const std::string& action) {
    return action.empty() || action == std::string(1, '\0');
}

bool IsPlaying(const std::string& action) {
    return !IsIdle(action) && action != "enable";
}

const char* Show(const std::string& action) {
    if (IsIdle(action)) {
        return "\\0";
    }
    return action.c_str();
}

}  // namespace

int main(int argc, char** argv) {
    dobot::sdk::SessionConfig config;
    int seconds = 0;
    bool print_all = false;
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
        } else if (arg == "--all") {
            print_all = true;
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto fsm = session.Subscribe<dobot::msg::dds_::SetFsmId_PubSubType>(dobot::robot::sr_3588::kTopicSetFsmId);
    if (!fsm.valid()) {
        std::cerr << "创建订阅失败\n";
        return 1;
    }

    std::mutex mu;
    bool seen = false;
    std::string previous;
    fsm.On([&](const dobot::msg::dds_::SetFsmId_& msg) {
        const std::string action = msg.current_action();
        const uint16_t fsm_id = msg.id();
        std::lock_guard<std::mutex> lock(mu);
        if (print_all) {
            std::cout << "收到 id=" << fsm_id << " current_action=" << Show(action) << "\n";
        }
        if (!seen) {
            seen = true;
            previous = action;
            if (IsPlaying(action)) {
                std::cout << "动作已在进行 " << action << " 状态机 " << fsm_id << "\n";
            }
            return;
        }
        const bool was_playing = IsPlaying(previous);
        const bool now_playing = IsPlaying(action);
        if (!was_playing && now_playing) {
            std::cout << "动作开始 " << action << " 状态机 " << fsm_id << "\n";
        } else if (was_playing && !now_playing) {
            std::cout << "动作结束 " << previous << " -> " << Show(action) << " 状态机 " << fsm_id << "\n";
        } else if (was_playing && now_playing && previous != action) {
            std::cout << "动作结束 " << previous << " 状态机 " << fsm_id << "\n";
            std::cout << "动作开始 " << action << " 状态机 " << fsm_id << "\n";
        }
        previous = action;
    });

    std::cout << "已订阅 " << dobot::robot::sr_3588::kTopicSetFsmId << "。另开程序播放技能。";
    if (print_all) {
        std::cout << "将打印每一帧。\n";
    } else {
        std::cout << "只在技能开始和结束时打印。\n";
    }
    const auto deadline = seconds > 0 ? std::chrono::steady_clock::now() + std::chrono::seconds(seconds)
                                       : std::chrono::steady_clock::time_point::max();
    while (g_stop == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}
