#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dobot/sdk/motion_rpc.hpp"

namespace dobot::motion {

// Locomotion modes that can run continuously. Enum values are independent of the underlying FSM ids; the mapping is kept in the implementation.
enum class RobotMode {
    kZeroTorque,   // zero torque
    kDamping,      // damping
    kCustomDebug,  // low-level debug
    kReady,        // ready stand
    kStanding,     // standing (Mini standing 300; rt/stand/cmd can be sent)
    kWalking,      // humanoid walking
};

// One entry of GetFsmList. The dance fields are populated only when is_dance is true.
struct FsmInfo {
    int32_t fsm_id = 0;
    bool is_dance = false;
    std::string name;
    std::string dance_name;
    std::string model_file;
    int motion_loop_length = 0;
    int motion_hold_back_steps = 0;
    int motion_start_step = 0;
};

// Semantic locomotion API. Calls MotionRpc internally; returns 0 on success, otherwise the integer value of dobot::sdk::RpcError.
class Motion {
public:
    explicit Motion(const dobot::sdk::MotionRpc& rpc) : rpc_(&rpc) {}

    // Switch the locomotion mode. Only means the RPC transition was accepted; does not guarantee the transition animation has finished.
    int32_t SetRobotMode(RobotMode mode, int timeout_ms = 3000) const;

    // Query the current FSM id. On success, writes fsm_id and returns 0. 0 is a valid id.
    int32_t GetFsmId(int32_t* fsm_id, int timeout_ms = 3000) const;

    // Query all FSMs. On success, writes fsm_list and returns 0. Returns kJson if a field is missing or has the wrong type.
    int32_t GetFsmList(std::vector<FsmInfo>* fsm_list, int timeout_ms = 3000) const;

    // Translate a known FSM id into a short string; unknown ids return "unknown(<id>)". The returned value can be stored.
    static std::string FsmIdToString(int32_t fsm_id);

    // Auto enter box. When finished, locomotion automatically enters damping mode (1). Success only means the transition was accepted.
    int32_t AutoEnterBox(int timeout_ms = 3000) const;
    // Lie down. Automatically enters damping (1) when finished.
    int32_t LieDown(int timeout_ms = 3000) const;
    // Lie prone. Automatically enters damping (1) when finished.
    int32_t GoProne(int timeout_ms = 3000) const;

    // Auto exit box. When finished, first enters humanoid walking (304), then automatically enters standing (300) after about 2 s. Do not call SetRobotMode(Standing) right after the RPC returns.
    int32_t AutoExitBox(int timeout_ms = 3000) const;
    // Get up from supine. Ends the same way as AutoExitBox.
    int32_t StandUpFromSupine(int timeout_ms = 3000) const;
    // Get up from prone. Ends the same way as AutoExitBox.
    int32_t StandUpFromProne(int timeout_ms = 3000) const;

    // Play a dance. fsm_id must be in 350-400. Only means SetFsmId was accepted.
    int32_t PlayDance(int32_t fsm_id, int timeout_ms = 3000) const;

    // Send chassis velocity. vx is positive forward, in m/s; vy is positive left, in m/s; vyaw is positive counterclockwise, in rad/s.
    // duration is the hold time in seconds; motion stops after it expires. To stop, use SetVelocity(0, 0, 0).
    int32_t SetVelocity(float vx, float vy, float vyaw, float duration = 1.0f, int timeout_ms = 3000) const;

private:
    int32_t TriggerFsm(int32_t fsm_id, int timeout_ms) const;

    const dobot::sdk::MotionRpc* rpc_;
};

}  // namespace dobot::motion
