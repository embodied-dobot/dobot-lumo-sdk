// SR_thor state subscription example. Use Latest() for periodic data; do no processing in the callback.
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
    std::cerr << "Usage: " << argv0 << " [--seconds 5] [--on-robot] [--domain 0] [--nic <nic>]\n"
              << "Subscribes to rt/lower/state and rt/upper/state and prints the received joint angles.\n"
              << "DDS domain defaults to 0. All network interfaces are used by default; with several, add --nic.\n";
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
                std::cerr << "Missing argument: " << name << "\n";
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
            std::cerr << "Unknown argument: " << arg << "\n";
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
        std::cerr << "Failed to create subscriber\n";
        return 1;
    }

    std::cout << "Waiting for rt/lower/state and rt/upper/state, up to " << seconds << " s\n";
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
        std::cerr << "No state received. Check that --domain matches the robot. With several network interfaces, use --nic to pick the one connected to the robot.\n";
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
