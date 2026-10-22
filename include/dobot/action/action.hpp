#pragma once

#include <string>

#include "dobot/sdk/motion_rpc.hpp"

namespace dobot::action {

// ExecuteAction 这一层能确定的结果。完成与否不在这里，机器人还没有进度反馈。
enum class ActionStatus {
    kNotSent,   // 请求没有到达，或没有拿到可解析的应答
    kRejected,  // 机器人有应答，但没有受理
    kAccepted,  // 机器人已受理。动作是否做完仍然未知
};

// 一次动作调用的结果。用户看 status，不看 JSON。
struct ActionResult {
    ActionStatus status = ActionStatus::kNotSent;
    dobot::sdk::RpcError error = dobot::sdk::RpcError::kOk;
    std::string message;

    bool sent() const { return status != ActionStatus::kNotSent; }
    bool accepted() const { return status == ActionStatus::kAccepted; }
};

const char* ActionStatusName(ActionStatus status);

// 动作业务。内部调用 MotionRpc::ExecuteAction，不新增 RPC。
class Action {
public:
    explicit Action(const dobot::sdk::MotionRpc& rpc) : rpc_(&rpc) {}

    // file_path 是机器人技能目录里的文件名。timeout_ms 只覆盖这次 RPC。
    // 返回 kAccepted 时，只表示机器人受理，不表示动作已经播完。
    ActionResult Execute(const std::string& file_path, int timeout_ms = 3000) const;

private:
    const dobot::sdk::MotionRpc* rpc_;
};

}  // namespace dobot::action
