#pragma once

namespace dobot::sdk {

// 一台机器人的连接描述。IP 和 RPC 端口放在这里。
// 两台机器行为相同时共用同一份实现，只换这份描述。
struct RobotProfile {
    const char* model = "";
    const char* default_robot_ip = "";
    int rpc_port = 0;
};

}  // namespace dobot::sdk
