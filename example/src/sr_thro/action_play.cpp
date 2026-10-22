// SR_thro 的动作示例。走业务层，不打印 JSON。
// 已受理只表示机器人收下了这次调用，不表示动作已经播完。
#include "dobot/action/action.hpp"
#include "dobot/robot/sr_thro/topics.hpp"
#include "dobot/sdk/session.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " --file <技能目录下的文件名>"
              << " [--timeout-ms 3000] [--ip " << dobot::robot::sr_thro::kDefaultRobotIp
              << "] [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "会在机器人上播放该动作。已受理不等于播放完成。\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string file_path;
    bool ip_set = false;
    bool on_robot = false;
    uint32_t domain_id = 0;
    std::string network_interface;
    std::string ip;
    int timeout_ms = 3000;

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
        } else if (arg == "--file") {
            file_path = need("--file");
        } else if (arg == "--timeout-ms") {
            timeout_ms = std::stoi(need("--timeout-ms"));
        } else if (arg == "--ip") {
            ip = need("--ip");
            ip_set = true;
        } else if (arg == "--on-robot") {
            on_robot = true;
        } else if (arg == "--domain") {
            domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            network_interface = need("--nic");
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }
    if (file_path.empty()) {
        Usage(argv[0]);
        return 2;
    }

    dobot::sdk::SessionConfig config;
    config.where = on_robot ? dobot::sdk::RunWhere::kRobot : dobot::sdk::RunWhere::kComputer;
    if (ip_set) {
        config.robot_ip = ip;
    }
    config.domain_id = domain_id;
    config.network_interface = network_interface;
    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thro::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    const dobot::action::ActionResult result = session.action().Execute(file_path, timeout_ms);
    std::cout << "动作 " << file_path << "：" << dobot::action::ActionStatusName(result.status);
    if (!result.message.empty()) {
        std::cout << "（" << result.message << "）";
    }
    std::cout << "\n";
    return result.accepted() ? 0 : 1;
}
