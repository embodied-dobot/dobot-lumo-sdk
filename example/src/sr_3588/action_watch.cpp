// SR_3588: Subscribes to rt/set/fsm/id and prints only when a skill starts or ends.
// This topic is published by the motion control algorithm. current_action empty or "\0" means nothing can be played,
// "enable" means playable but idle; any other non-empty string is the skill currently playing.
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
    std::cerr << "Usage: " << argv0 << " [--all] [--seconds 0] [--on-robot] [--domain 0] [--nic <nic>]\n"
              << "Subscribes to rt/set/fsm/id. Prints start when a skill name appears, and end when it returns to enable or empty.\n"
              << "With --all, every frame is printed, even if the state did not change.\n"
              << "--seconds 0 runs until Ctrl+C. Do not call RPC or Shutdown inside the callback.\n";
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
        } else if (arg == "--all") {
            print_all = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
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
        std::cerr << "Failed to create subscriber\n";
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
            std::cout << "Received id=" << fsm_id << " current_action=" << Show(action) << "\n";
        }
        if (!seen) {
            seen = true;
            previous = action;
            if (IsPlaying(action)) {
                std::cout << "Action already in progress " << action << " FSM " << fsm_id << "\n";
            }
            return;
        }
        const bool was_playing = IsPlaying(previous);
        const bool now_playing = IsPlaying(action);
        if (!was_playing && now_playing) {
            std::cout << "Action started " << action << " FSM " << fsm_id << "\n";
        } else if (was_playing && !now_playing) {
            std::cout << "Action ended " << previous << " -> " << Show(action) << " FSM " << fsm_id << "\n";
        } else if (was_playing && now_playing && previous != action) {
            std::cout << "Action ended " << previous << " FSM " << fsm_id << "\n";
            std::cout << "Action started " << action << " FSM " << fsm_id << "\n";
        }
        previous = action;
    });

    std::cout << "Subscribed to " << dobot::robot::sr_3588::kTopicSetFsmId << ". Play a skill from another program. ";
    if (print_all) {
        std::cout << "Every frame will be printed.\n";
    } else {
        std::cout << "Printing only when a skill starts or ends.\n";
    }
    const auto deadline = seconds > 0 ? std::chrono::steady_clock::now() + std::chrono::seconds(seconds)
                                       : std::chrono::steady_clock::time_point::max();
    while (g_stop == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}
