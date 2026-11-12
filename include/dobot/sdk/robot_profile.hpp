#pragma once

namespace dobot::sdk {

// Connection profile of one robot. The IP and RPC port live here.
// Robots with identical behavior share one implementation and differ only in this profile.
struct RobotProfile {
    const char* model = "";
    const char* default_robot_ip = "";
    int rpc_port = 0;
};

}  // namespace dobot::sdk
