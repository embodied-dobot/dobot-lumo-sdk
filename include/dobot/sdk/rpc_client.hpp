#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace dobot::sdk {

// Error codes consistent with the onboard JSON-RPC client. 0-6 are transport and generic failures; 100-106 are locomotion business failures. Shared by all models.
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

    // Call succeeded: error code is 0 and the response contains result. False when result is -1, on a JSON-RPC error, or when a locomotion method reports a business failure.
    bool ok() const { return error == RpcError::kOk && response.contains("result"); }
    // Get result from the response. Throws if the field is missing; check ok() first.
    const nlohmann::json& result() const { return response.at("result"); }
};

// Each call opens a new TCP connection, sends one line of JSON-RPC 2.0, and reads until a newline or a complete JSON object.
// Transport only. Methods shared by both robots live in MotionRpc, obtained from Session::rpc().
class RpcClient {
public:
    // Store the robot address. No connection is made at construction; each Call connects.
    RpcClient(std::string ip, int port);

    // Make one JSON-RPC 2.0 call. A null params is sent as an empty object {}.
    // timeout_ms covers connect, send and receive, in milliseconds.
    // error is kServerInternal when result is the integer -1. error is kUnexpected when the response has only a JSON-RPC error and no result.
    RpcResult Call(const std::string& method, const nlohmann::json& params, int timeout_ms = 3000) const;

private:
    std::string ip_;
    int port_;
};

// Convert an RpcError to a stable short string. Unknown values return "unknown".
const char* RpcErrorName(RpcError error);

}  // namespace dobot::sdk
