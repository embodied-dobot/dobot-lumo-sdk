// SR_thor 的状态订阅示例。周期数据用 Latest()，不在回调里做处理。
#include "dobot/robot/sr_thor/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "lower_state.hpp"
#include "lower_statePubSubTypes.hpp"
#include "upper_state.hpp"
#include "upper_statePubSubTypes.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

namespace dds_ = dobot::msg::dds_;

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " [--seconds 5] [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "订阅 rt/lower/state 和 rt/upper/state，打印收到的关节角。\n"
              << "DDS domain 默认 0。网卡默认全部，多网卡时加 --nic。\n";
}

template <typename Msg>
void PrintMotors(const char* title, const Msg& msg) {
    std::cout << title << " joints=" << msg.motor_state().size() << "\n";
    const size_t n = std::min<size_t>(msg.motor_state().size(), 4);
    for (size_t i = 0; i < n; ++i) {
        const auto& motor = msg.motor_state()[i];
        std::cout << "  [" << i << "] mode=" << static_cast<unsigned>(motor.mode()) << " q=" << motor.q() << " dq=" << motor.dq()
                  << " tau_est=" << motor.tau_est() << "\n";
    }
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

    auto lower = session.Subscribe<dds_::LowerState_PubSubType>(dobot::robot::sr_thor::kTopicLowerState);
    auto upper = session.Subscribe<dds_::UpperState_PubSubType>(dobot::robot::sr_thor::kTopicUpperState);
    if (!lower.valid() || !upper.valid()) {
        std::cerr << "创建订阅失败\n";
        return 1;
    }

    std::cout << "等待 rt/lower/state 和 rt/upper/state，最长 " << seconds << " 秒\n";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (lower.Latest() && upper.Latest()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "matched lower=" << lower.Matched() << " upper=" << upper.Matched() << "\n";
    const auto lower_sample = lower.Latest();
    const auto upper_sample = upper.Latest();
    if (!lower_sample && !upper_sample) {
        std::cerr << "没有收到状态。请确认 --domain 与机器人一致。多网卡时用 --nic 指定接机器人的网卡。\n";
        return 1;
    }
    if (lower_sample) {
        PrintMotors("lower", *lower_sample);
    }
    if (upper_sample) {
        std::string robot_type;
        for (char ch : upper_sample->robot_type()) {
            if (ch == '\0') {
                break;
            }
            robot_type.push_back(ch);
        }
        std::cout << "upper robot_type=" << robot_type << " upper_control=" << std::boolalpha << upper_sample->is_upper_control() << "\n";
        PrintMotors("upper", *upper_sample);
    }
    return (lower_sample && upper_sample) ? 0 : 1;
}
