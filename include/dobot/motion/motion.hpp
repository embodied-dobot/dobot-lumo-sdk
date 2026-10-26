#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dobot/sdk/motion_rpc.hpp"

namespace dobot::motion {

// 可持续运行的运控模式。枚举值与底层状态机编号无关，映射在实现里维护。
enum class RobotMode {
    kZeroTorque,   // 零力矩
    kDamping,      // 阻尼
    kCustomDebug,  // 底层调试
    kReady,        // 预备站立
    kStanding,     // 站立（Mini 站立 300，可发 rt/stand/cmd）
    kWalking,      // 拟人行走
};

// GetFsmList 的一项。舞蹈字段只在 is_dance 为 true 时有内容。
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

// 语义化运控接口。内部调用 MotionRpc，成功返回 0，否则返回 dobot::sdk::RpcError 整型值。
class Motion {
public:
    explicit Motion(const dobot::sdk::MotionRpc& rpc) : rpc_(&rpc) {}

    // 切换运控模式。只表示 RPC 切换被接受，不保证过渡动画已结束。
    int32_t SetRobotMode(RobotMode mode, int timeout_ms = 3000) const;

    // 查询当前状态机编号。成功时写入 fsm_id 并返回 0。0 也是合法编号。
    int32_t GetFsmId(int32_t* fsm_id, int timeout_ms = 3000) const;

    // 查询全部状态机。成功时写入 fsm_list 并返回 0。字段缺失或类型不对时返回 kJson。
    int32_t GetFsmList(std::vector<FsmInfo>* fsm_list, int timeout_ms = 3000) const;

    // 把已知状态机编号翻译成短字符串；未知编号返回 "unknown(<id>)"。返回值可以保存。
    static std::string FsmIdToString(int32_t fsm_id);

    // 自主入箱。完成后运控会自动进入阻尼模式（1）。成功只表示切换被接受。
    int32_t AutoEnterBox(int timeout_ms = 3000) const;
    // 躺下。完成后自动进入阻尼（1）。
    int32_t LieDown(int timeout_ms = 3000) const;
    // 趴下。完成后自动进入阻尼（1）。
    int32_t GoProne(int timeout_ms = 3000) const;

    // 自主出箱。完成后先进入拟人行走（304），约 2 s 后自动进入站立（300）。勿在 RPC 返回后立刻 SetRobotMode(Standing)。
    int32_t AutoExitBox(int timeout_ms = 3000) const;
    // 仰姿起身。收尾同 AutoExitBox。
    int32_t StandUpFromSupine(int timeout_ms = 3000) const;
    // 俯姿起身。收尾同 AutoExitBox。
    int32_t StandUpFromProne(int timeout_ms = 3000) const;

    // 播放舞蹈。fsm_id 须在 350–400。只表示 SetFsmId 被接受。
    int32_t PlayDance(int32_t fsm_id, int timeout_ms = 3000) const;

    // 下发底盘速度。vx 向前为正，m/s；vy 向左为正，m/s；vyaw 逆时针为正，rad/s。
    // duration 是保持时间，单位秒，超时后停止。停机用 SetVelocity(0, 0, 0)。
    int32_t SetVelocity(float vx, float vy, float vyaw, float duration = 1.0f, int timeout_ms = 3000) const;

private:
    int32_t TriggerFsm(int32_t fsm_id, int timeout_ms) const;

    const dobot::sdk::MotionRpc* rpc_;
};

}  // namespace dobot::motion
