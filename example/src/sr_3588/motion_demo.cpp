// SR_3588 语义化运控示例：查询当前状态机、列出全部状态机、切换 RobotMode。
#include "dobot/motion/motion.hpp"
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/rpc_client.hpp"
#include "dobot/sdk/session.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " [--query] [--list] [--mode zero|damp|debug|ready|stand|walk]"
              << " [--timeout-ms 3000] [--ip " << dobot::robot::sr_3588::kDefaultRobotIp
              << "] [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "默认打印当前状态机。--list 打印全部状态机编号。指定 --mode 时先切换，再打印当前状态机。\n"
              << "示例: " << argv0 << " --list\n"
              << "      " << argv0 << " --mode stand\n";
}

bool ParseMode(const std::string& name, dobot::motion::RobotMode* out) {
    if (name == "zero") {
        *out = dobot::motion::RobotMode::kZeroTorque;
        return true;
    }
    if (name == "damp") {
        *out = dobot::motion::RobotMode::kDamping;
        return true;
    }
    if (name == "debug") {
        *out = dobot::motion::RobotMode::kCustomDebug;
        return true;
    }
    if (name == "ready") {
        *out = dobot::motion::RobotMode::kReady;
        return true;
    }
    if (name == "stand") {
        *out = dobot::motion::RobotMode::kStanding;
        return true;
    }
    if (name == "walk") {
        *out = dobot::motion::RobotMode::kWalking;
        return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    bool query_requested = false;
    bool list = false;
    bool mode_set = false;
    bool ip_set = false;
    bool on_robot = false;
    uint32_t domain_id = 0;
    std::string network_interface;
    std::string mode_name;
    std::string ip;
    int timeout_ms = 3000;
    dobot::motion::RobotMode mode = dobot::motion::RobotMode::kStanding;

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
        } else if (arg == "--query") {
            query_requested = true;
        } else if (arg == "--list") {
            list = true;
        } else if (arg == "--mode") {
            mode_name = need("--mode");
            if (!ParseMode(mode_name, &mode)) {
                std::cerr << "未知模式: " << mode_name << "\n";
                return 2;
            }
            mode_set = true;
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

    const dobot::motion::Motion motion = session.motion();
    if (mode_set) {
        const int32_t code = motion.SetRobotMode(mode, timeout_ms);
        if (code != 0) {
            std::cerr << "SetRobotMode 失败 " << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
            return 1;
        }
        std::cout << "SetRobotMode(" << mode_name << ") 成功\n";
    }

    const bool do_query = query_requested || mode_set || (!list && !mode_set);
    if (do_query) {
        int32_t fsm_id = 0;
        const int32_t code = motion.GetFsmId(&fsm_id, timeout_ms);
        if (code != 0) {
            std::cerr << "GetFsmId 失败 " << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
            return 1;
        }
        std::cout << "当前状态机 " << fsm_id << " (" << dobot::motion::Motion::FsmIdToString(fsm_id) << ")\n";
    }

    if (list) {
        std::vector<dobot::motion::FsmInfo> fsm_list;
        const int32_t code = motion.GetFsmList(&fsm_list, timeout_ms);
        if (code != 0) {
            std::cerr << "GetFsmList 失败 " << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
            return 1;
        }
        std::cout << "状态机列表\n";
        for (const dobot::motion::FsmInfo& item : fsm_list) {
            std::cout << item.fsm_id << " " << item.name;
            if (item.is_dance) {
                std::cout << " 舞蹈 " << item.dance_name << " " << item.model_file;
            }
            std::cout << "\n";
        }
    }

    return 0;
}
