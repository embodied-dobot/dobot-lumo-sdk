// SR_3588 的状态机示例。走封装的 SetFsmId，把运控状态机切到指定编号。
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " --fsm-id <状态机编号>"
              << " [--timeout-ms 3000] [--ip " << dobot::robot::sr_3588::kDefaultRobotIp
              << "] [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "调用 SetFsmId 切换运控状态机。成功时 result 为 1。\n"
              << "不填 --ip 时，电脑上用机型默认地址，--on-robot 时用 127.0.0.1。\n"
              << "示例: " << argv0 << " --fsm-id 300\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool fsm_set = false;
    bool ip_set = false;
    bool on_robot = false;
    uint32_t domain_id = 0;
    std::string network_interface;
    int32_t fsm_id = 0;
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
        } else if (arg == "--fsm-id") {
            fsm_id = static_cast<int32_t>(std::stoi(need("--fsm-id")));
            fsm_set = true;
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
    if (!fsm_set) {
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
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    const dobot::sdk::RpcResult result = session.rpc().SetFsmId(fsm_id, timeout_ms);
    if (!result.ok()) {
        std::cerr << "SetFsmId(" << fsm_id << ") 失败 " << dobot::sdk::RpcErrorName(result.error) << ": " << result.message
                  << "\n";
        if (!result.response.is_null() && !result.response.empty()) {
            std::cerr << result.response.dump() << "\n";
        }
        return 1;
    }
    std::cout << "状态机已设置为 " << fsm_id << "，result=" << result.result() << "\n";
    return 0;
}
