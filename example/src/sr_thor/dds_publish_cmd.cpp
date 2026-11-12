// SR_thor lower-body command example. The command is written from the application thread, not through the callback queue.
#include "dobot/robot/sr_thor/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "lower_cmd.hpp"
#include "lower_cmdPubSubTypes.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void Usage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " --send [--on-robot] [--domain 0] [--nic <nic>]\n"
              << "Sends one all-zero LowerCmd frame to rt/lower/cmd to confirm the writer can match the robot.\n"
              << "DDS domain defaults to 0. All network interfaces are used by default; with several, add --nic.\n"
              << "This is a joint command. Add --send only when you really intend a communication test.\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool send = false;
    dobot::sdk::SessionConfig config;
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
        } else if (arg == "--send") {
            send = true;
        } else if (arg == "--on-robot") {
            config.where = dobot::sdk::RunWhere::kRobot;
        } else if (arg == "--domain") {
            config.domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            config.network_interface = need("--nic");
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }
    if (!send) {
        Usage(argv[0]);
        return 2;
    }

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thor::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto writer = session.Publish<dobot::msg::dds_::LowerCmd_PubSubType>(dobot::robot::sr_thor::kTopicLowerCmd);
    if (!writer.valid()) {
        std::cerr << "Failed to create publisher\n";
        return 1;
    }

    dobot::msg::dds_::LowerCmd_ cmd;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && writer.Matched() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    const int matched = writer.Matched();
    std::cout << "rt/lower/cmd matched=" << matched << "\n";

    if (!writer.Write(cmd)) {
        std::cerr << "write failed\n";
        return 1;
    }
    std::cout << "Sent one all-zero LowerCmd frame\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return matched > 0 ? 0 : 1;
}
