#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace dobot::sdk {

// 和机载 JSON-RPC 客户端一致的错误码。0–6 是传输和通用失败，100–106 是运控业务失败。各型号共用。
enum class RpcError {
    kOk = 0,
    kSocket = 1,
    kConnect = 2,
    kNoResponse = 3,
    kJson = 4,
    kUnexpected = 5,
    kServerInternal = 6,
    kFsmTransitionFailed = 100,
    kSwitchUpperLimbControlFailed = 101,
    kSetVelFailed = 102,
    kSetUpperLimbJointFailed = 103,
    kSetLiftPoseFailed = 104,
    kTriggerTrajectoryRecordingFailed = 105,
    kExecuteActionFailed = 106,
};

struct RpcResult {
    RpcError error = RpcError::kOk;
    std::string message;
    nlohmann::json response = nlohmann::json::object();

    // 调用成功：错误码为 0，且响应里有 result。result 为 -1、JSON-RPC error，或运控方法判定业务失败时为 false。
    bool ok() const { return error == RpcError::kOk && response.contains("result"); }
    // 取出响应里的 result。没有该字段时抛出异常，调用前先用 ok() 判断。
    const nlohmann::json& result() const { return response.at("result"); }
};

// 每次调用新建一条 TCP 连接，发送一行 JSON-RPC 2.0，读到换行或一个完整 JSON 对象。
// 只负责传输。两台机器人共用的方法在 MotionRpc，由 Session::rpc() 给出。
class RpcClient {
public:
    // 保存机器人地址。构造时不建连接，连接发生在每次 Call 里。
    RpcClient(std::string ip, int port);

    // 发起一次 JSON-RPC 2.0 调用。params 为 null 时按空对象 {} 发送。
    // timeout_ms 覆盖连接、发送和接收，单位毫秒。
    // result 为整数 -1 时 error 为 kServerInternal。响应只有 JSON-RPC error、没有 result 时 error 为 kUnexpected。
    RpcResult Call(const std::string& method, const nlohmann::json& params, int timeout_ms = 3000) const;

private:
    std::string ip_;
    int port_;
};

// 把 RpcError 转成稳定短字符串。未知值返回 "unknown"。
const char* RpcErrorName(RpcError error);

}  // namespace dobot::sdk
