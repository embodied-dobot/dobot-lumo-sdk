// SR_thro 的原始 JSON-RPC 示例。
// 查状态机、切模式、预设动作、舞蹈用 motion_demo；速度用 walk_demo。
// 这里留给 session.rpc() 还没有单独示例的方法，以及还没有成员函数的方法。
#include "dobot/robot/sr_thro/topics.hpp"
#include "dobot/sdk/session.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Usage(const char* argv0) {
    std::cerr
        << "用法: " << argv0
        << " [--ip " << dobot::robot::sr_thro::kDefaultRobotIp << "] [--port " << dobot::robot::sr_thro::kAlgsRpcPort
        << "] --method 方法名 [--params '{}'] [--timeout-ms 3000] [--on-robot] [--domain 0] [--nic 网卡名]\n"
        << "不填 --ip 时，电脑上用机型默认地址，--on-robot 时用 127.0.0.1。\n"
        << "查状态机、切模式、预设动作和舞蹈用 motion_demo，速度用 walk_demo。\n"
        << "示例: " << argv0 << " --method ReloadArmSkillsData\n"
        << "      " << argv0 << " --method ExecuteAction --params '{\"file_path\":\"skill.csv\"}'\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool ip_set = false;
    bool on_robot = false;
    uint32_t domain_id = 0;
    std::string network_interface;
    std::string ip;
    int port = dobot::robot::sr_thro::kAlgsRpcPort;
    bool method_set = false;
    std::string method;
    std::string params_text = "{}";
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
        } else if (arg == "--on-robot") {
            on_robot = true;
        } else if (arg == "--domain") {
            domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            network_interface = need("--nic");
        } else if (arg == "--ip") {
            ip = need("--ip");
            ip_set = true;
        } else if (arg == "--port") {
            port = std::stoi(need("--port"));
        } else if (arg == "--method") {
            method = need("--method");
            method_set = true;
        } else if (arg == "--params") {
            params_text = need("--params");
        } else if (arg == "--timeout-ms") {
            timeout_ms = std::stoi(need("--timeout-ms"));
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }
    if (!method_set || method.empty()) {
        std::cerr << "缺少 --method。查状态机和切模式请用 motion_demo，速度请用 walk_demo。\n";
        Usage(argv[0]);
        return 2;
    }

    nlohmann::json params;
    try {
        params = nlohmann::json::parse(params_text);
    } catch (const std::exception& ex) {
        std::cerr << "params 不是 JSON: " << ex.what() << "\n";
        return 2;
    }

    dobot::sdk::SessionConfig config;
    config.where = on_robot ? dobot::sdk::RunWhere::kRobot : dobot::sdk::RunWhere::kComputer;
    if (ip_set) {
        config.robot_ip = ip;
    }
    config.domain_id = domain_id;
    config.network_interface = network_interface;
    config.rpc_port = port;

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thro::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    const dobot::sdk::RpcResult result = session.Call(method, params, timeout_ms);
    if (result.error != dobot::sdk::RpcError::kOk) {
        std::cerr << "RPC 失败 " << dobot::sdk::RpcErrorName(result.error) << ": " << result.message << "\n";
        if (!result.response.is_null() && !result.response.empty()) {
            std::cerr << result.response.dump() << "\n";
        }
        return 1;
    }
    std::cout << result.response.dump(2) << "\n";
    return 0;
}
