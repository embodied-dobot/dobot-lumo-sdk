// SR_3588 语义化运控示例：查询状态机、切换模式、预设动作、舞蹈。
// 已知编号走 session.motion()。对不上这些接口的编号才走 session.rpc().SetFsmId。
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
    std::cerr
        << "用法: " << argv0
        << " [--query] [--list]"
        << " [--mode zero|damp|debug|ready|stand|walk]"
        << " [--action enter-box|lie|prone|exit-box|stand-supine|stand-prone]"
        << " [--dance 350-400] [--fsm-id 编号]"
        << " [--timeout-ms 3000] [--ip " << dobot::robot::sr_3588::kDefaultRobotIp
        << "] [--on-robot] [--domain 0] [--nic 网卡名]\n"
        << "默认打印当前状态机。--list 打印全部状态机。\n"
        << "--mode、--action、--dance、--fsm-id 只能指定一个，执行后再打印当前状态机。\n"
        << "--mode 走 SetRobotMode，--action 走六个预设动作，--dance 走 PlayDance。\n"
        << "--fsm-id 能对应到上述接口时走对应接口，其余编号走 rpc().SetFsmId。\n"
        << "自主出箱、仰姿起身、俯姿起身返回后不要立刻再切到站立。\n"
        << "示例: " << argv0 << "\n"
        << "      " << argv0 << " --list\n"
        << "      " << argv0 << " --mode stand\n"
        << "      " << argv0 << " --action lie\n"
        << "      " << argv0 << " --dance 350\n"
        << "      " << argv0 << " --fsm-id 300\n";
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

using ActionFn = int32_t (dobot::motion::Motion::*)(int) const;

bool ParseAction(const std::string& name, ActionFn* out, const char** label) {
    if (name == "enter-box") {
        *out = &dobot::motion::Motion::AutoEnterBox;
        *label = "AutoEnterBox";
        return true;
    }
    if (name == "lie") {
        *out = &dobot::motion::Motion::LieDown;
        *label = "LieDown";
        return true;
    }
    if (name == "prone") {
        *out = &dobot::motion::Motion::GoProne;
        *label = "GoProne";
        return true;
    }
    if (name == "exit-box") {
        *out = &dobot::motion::Motion::AutoExitBox;
        *label = "AutoExitBox";
        return true;
    }
    if (name == "stand-supine") {
        *out = &dobot::motion::Motion::StandUpFromSupine;
        *label = "StandUpFromSupine";
        return true;
    }
    if (name == "stand-prone") {
        *out = &dobot::motion::Motion::StandUpFromProne;
        *label = "StandUpFromProne";
        return true;
    }
    return false;
}

int32_t SwitchByFsmId(const dobot::sdk::Session& session, const dobot::motion::Motion& motion, int32_t fsm_id,
                      int timeout_ms, std::string* label) {
    switch (fsm_id) {
        case 0:
            *label = "SetRobotMode(zero)";
            return motion.SetRobotMode(dobot::motion::RobotMode::kZeroTorque, timeout_ms);
        case 1:
            *label = "SetRobotMode(damp)";
            return motion.SetRobotMode(dobot::motion::RobotMode::kDamping, timeout_ms);
        case 2:
            *label = "SetRobotMode(debug)";
            return motion.SetRobotMode(dobot::motion::RobotMode::kCustomDebug, timeout_ms);
        case 100:
            *label = "SetRobotMode(ready)";
            return motion.SetRobotMode(dobot::motion::RobotMode::kReady, timeout_ms);
        case 300:
            *label = "SetRobotMode(stand)";
            return motion.SetRobotMode(dobot::motion::RobotMode::kStanding, timeout_ms);
        case 304:
            *label = "SetRobotMode(walk)";
            return motion.SetRobotMode(dobot::motion::RobotMode::kWalking, timeout_ms);
        case 307:
            *label = "AutoEnterBox";
            return motion.AutoEnterBox(timeout_ms);
        case 308:
            *label = "AutoExitBox";
            return motion.AutoExitBox(timeout_ms);
        case 310:
            *label = "LieDown";
            return motion.LieDown(timeout_ms);
        case 311:
            *label = "StandUpFromSupine";
            return motion.StandUpFromSupine(timeout_ms);
        case 312:
            *label = "GoProne";
            return motion.GoProne(timeout_ms);
        case 313:
            *label = "StandUpFromProne";
            return motion.StandUpFromProne(timeout_ms);
        default:
            break;
    }
    if (fsm_id >= 350 && fsm_id <= 400) {
        *label = "PlayDance(" + std::to_string(fsm_id) + ")";
        return motion.PlayDance(fsm_id, timeout_ms);
    }
    *label = "SetFsmId(" + std::to_string(fsm_id) + ")";
    const dobot::sdk::RpcResult result = session.rpc().SetFsmId(fsm_id, timeout_ms);
    if (result.error == dobot::sdk::RpcError::kOk) {
        return 0;
    }
    return static_cast<int32_t>(result.error);
}

int Fail(const std::string& label, int32_t code) {
    std::cerr << label << " 失败 " << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    bool query_requested = false;
    bool list = false;
    bool mode_set = false;
    bool action_set = false;
    bool dance_set = false;
    bool fsm_set = false;
    bool ip_set = false;
    bool on_robot = false;
    uint32_t domain_id = 0;
    std::string network_interface;
    std::string mode_name;
    std::string ip;
    int timeout_ms = 3000;
    int32_t dance_id = 0;
    int32_t fsm_id = 0;
    dobot::motion::RobotMode mode = dobot::motion::RobotMode::kStanding;
    ActionFn action = nullptr;
    const char* action_label = "";

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
        } else if (arg == "--action") {
            const std::string action_name = need("--action");
            if (!ParseAction(action_name, &action, &action_label)) {
                std::cerr << "未知动作: " << action_name << "\n";
                return 2;
            }
            action_set = true;
        } else if (arg == "--dance") {
            dance_id = static_cast<int32_t>(std::stoi(need("--dance")));
            if (dance_id < 350 || dance_id > 400) {
                std::cerr << "--dance 须在 350–400\n";
                return 2;
            }
            dance_set = true;
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

    const int selected = (mode_set ? 1 : 0) + (action_set ? 1 : 0) + (dance_set ? 1 : 0) + (fsm_set ? 1 : 0);
    if (selected > 1) {
        std::cerr << "--mode、--action、--dance、--fsm-id 只能指定一个\n";
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

    const dobot::motion::Motion motion = session.motion();
    const bool switched = selected > 0;
    if (switched) {
        std::string label;
        int32_t code = 0;
        if (mode_set) {
            label = "SetRobotMode(" + mode_name + ")";
            code = motion.SetRobotMode(mode, timeout_ms);
        } else if (action_set) {
            label = action_label;
            code = (motion.*action)(timeout_ms);
        } else if (dance_set) {
            label = "PlayDance(" + std::to_string(dance_id) + ")";
            code = motion.PlayDance(dance_id, timeout_ms);
        } else {
            code = SwitchByFsmId(session, motion, fsm_id, timeout_ms, &label);
        }
        if (code != 0) {
            return Fail(label, code);
        }
        std::cout << label << " 成功\n";
    }

    const bool do_query = query_requested || switched || !list;
    if (do_query) {
        int32_t current_id = 0;
        const int32_t code = motion.GetFsmId(&current_id, timeout_ms);
        if (code != 0) {
            return Fail("GetFsmId", code);
        }
        std::cout << "当前状态机 " << current_id << " (" << dobot::motion::Motion::FsmIdToString(current_id) << ")\n";
    }

    if (list) {
        std::vector<dobot::motion::FsmInfo> fsm_list;
        const int32_t code = motion.GetFsmList(&fsm_list, timeout_ms);
        if (code != 0) {
            return Fail("GetFsmList", code);
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
