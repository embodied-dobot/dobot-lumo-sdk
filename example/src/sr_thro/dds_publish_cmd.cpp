// SR_thro 的下肢命令示例。命令在业务线程里 Write，不经过回调队列。
#include "dobot/robot/sr_thro/topics.hpp"
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
    std::cerr << "用法: " << argv0 << " --send [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "向 rt/lower/cmd 发一帧全 0 的 LowerCmd，用来确认写端能和机器人匹配。\n"
              << "DDS domain 默认 0。网卡默认全部，多网卡时加 --nic。\n"
              << "这是关节命令，只在明确要做通信测试时加 --send。\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool send = false;
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
        } else if (arg == "--send") {
            send = true;
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
    if (!send) {
        Usage(argv[0]);
        return 2;
    }

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thro::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto writer = session.Publish<dobot::msg::dds_::LowerCmd_PubSubType>(dobot::robot::sr_thro::kTopicLowerCmd);
    if (!writer.valid()) {
        std::cerr << "创建发布失败\n";
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
        std::cerr << "write 失败\n";
        return 1;
    }
    std::cout << "已发送一帧全 0 LowerCmd\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return matched > 0 ? 0 : 1;
}
