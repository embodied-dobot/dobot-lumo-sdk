#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "dobot/sdk/rpc_client.hpp"

namespace dobot::sdk {

class Session;

// Locomotion methods identical on both robots, implemented once.
// Add new methods shared by both models here. Methods specific to one model go in that model's directory and call Session::Call internally.
class MotionRpc {
public:
    MotionRpc() = default;

    // Send a JSON-RPC method directly. For new interfaces not yet wrapped as functions.
    RpcResult Call(const std::string& method, const nlohmann::json& params, int timeout_ms = 3000) const;

    // Query the current locomotion FSM id. On success, result is the id; 0 is a valid id.
    RpcResult GetFsmId(int timeout_ms = 3000) const;
    // Switch the locomotion FSM. result 1 means success; 0 means the transition failed.
    // Use 300 to enter Mini standing mode; only then can rt/stand/cmd be sent.
    RpcResult SetFsmId(int32_t fsm_id, int timeout_ms = 3000) const;
    // Query all current FSMs. On success, result is an array of objects. Returns kJson if it is not an array.
    RpcResult GetFsmList(int timeout_ms = 3000) const;
    // vx is positive forward, in m/s; vy is positive left, in m/s; vyaw is positive counterclockwise, in rad/s.
    // duration is the hold time in seconds; motion stops after it expires. To stop, use SetVel(0, 0, 0).
    RpcResult SetVel(float vx, float vy, float vyaw, float duration = 1.0f, int timeout_ms = 3000) const;
    // Reload the skill list. Before calling, put the trajectory CSV files in the skill directory on the robot.
    RpcResult ReloadArmSkillsData(int timeout_ms = 3000) const;
    // Send the mapping from keys to skill names. Only the keys present in this call are overwritten.
    RpcResult SendActionMapping(const std::map<std::string, std::string>& action_mapping, int timeout_ms = 3000) const;
    // Play an action from the skill directory. file_path is a file name, not a path on the caller's machine.
    // timeout_ms covers this RPC round trip only; it does not mean the action has finished playing.
    RpcResult ExecuteAction(const std::string& file_path, int timeout_ms = 3000) const;

private:
    friend class Session;
    void Bind(RpcClient* client) { client_ = client; }

    RpcClient* client_ = nullptr;
};

}  // namespace dobot::sdk
