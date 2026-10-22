#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "dobot/sdk/rpc_client.hpp"

namespace dobot::sdk {

class Session;

// 两台机器人相同的运控方法，只实现这一份。
// 两台都有的新方法加在这里。只有某一台才有的方法写到该型号目录，内部调用 Session::Call。
class MotionRpc {
public:
    MotionRpc() = default;

    // 直接发一个 JSON-RPC 方法。给还没封装成函数的新接口用。
    RpcResult Call(const std::string& method, const nlohmann::json& params, int timeout_ms = 3000) const;

    // 查询当前运控状态机编号。成功时 result 就是编号，0 也是合法编号。
    RpcResult GetFsmId(int timeout_ms = 3000) const;
    // 切换运控状态机。result 为 1 表示成功，0 表示切换失败。
    // 进入 Mini 站立模式使用 300，之后才能发 rt/stand/cmd。
    RpcResult SetFsmId(int32_t fsm_id, int timeout_ms = 3000) const;
    // vx 向前为正，m/s；vy 向左为正，m/s；vyaw 逆时针为正，rad/s。
    // duration 是保持时间，单位秒，超时后停止。停机用 SetVel(0, 0, 0)。
    RpcResult SetVel(float vx, float vy, float vyaw, float duration = 1.0f, int timeout_ms = 3000) const;
    // 重新加载技能列表。调用前把轨迹 csv 放到机器人上的技能目录。
    RpcResult ReloadArmSkillsData(int timeout_ms = 3000) const;
    // 下发按键到技能名的映射。只覆盖本次出现的键。
    RpcResult SendActionMapping(const std::map<std::string, std::string>& action_mapping, int timeout_ms = 3000) const;
    // 播放技能目录里的动作。file_path 是文件名，不是调用方本机路径。
    // timeout_ms 只覆盖这次 RPC 往返，不表示动作已经播完。
    RpcResult ExecuteAction(const std::string& file_path, int timeout_ms = 3000) const;
    // 登记一个舞蹈模型。成功时 result 为新的状态机编号，范围 350 到 400。之后用 SetFsmId 播放。
    RpcResult AddDancePolicy(const std::string& name, int motion_loop_length, int motion_hold_back_steps, int motion_start_step = 0, int timeout_ms = 3000) const;

private:
    friend class Session;
    void Bind(RpcClient* client) { client_ = client; }

    RpcClient* client_ = nullptr;
};

}  // namespace dobot::sdk
