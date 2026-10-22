#pragma once

#include "dobot/sdk/robot_profile.hpp"

namespace dobot::robot::sr_3588 {

// 型号 SR_3588。连接参数和话题名只属于这一台。
constexpr const char* kModel = "SR_3588";
constexpr int kAlgsRpcPort = 51234;
constexpr const char* kDefaultRobotIp = "192.168.5.2";

// 关节状态与命令。
constexpr const char* kTopicLowerState = "rt/lower/state";                 // LowerState_
constexpr const char* kTopicUpperState = "rt/upper/state";                 // UpperState_
constexpr const char* kTopicLowerCmd = "rt/lower/cmd";                     // LowerCmd_
constexpr const char* kTopicUpperCmd = "rt/upper/cmd";                     // UpperCmd_
constexpr const char* kTopicStandCmd = "rt/stand/cmd";                     // StandingControl_

// IMU。两路都是 IMUState_。一次会话只能订其中一路。
constexpr const char* kTopicPelvisImu = "rt/pelvis/imu/data";
constexpr const char* kTopicTorsoImu = "rt/torso/imu/data";

// 手柄、主节点。
constexpr const char* kTopicGamepadState = "rt/gamepad/state";             // GamepadState_
constexpr const char* kTopicMainNodesState = "rt/main/nodes/state";        // MainNodesState_

// 电池与 MCU 遥测。电量百分比在 BmsCommon_.remaining_pct。
constexpr const char* kTopicMcuBmsCommon = "Mcu/BmsCommon";                 // BmsCommon_
constexpr const char* kTopicMcuBmsUncommon = "Mcu/BmsUncommon";             // BmsUncommon_
constexpr const char* kTopicMcuHeartbeat = "Mcu/Heartbeat";                 // Heartbeat_
constexpr const char* kTopicMcuEstop = "Mcu/Estop";                         // Estop_
constexpr const char* kTopicMcuAdc = "Mcu/Adc";                             // Adc_
constexpr const char* kTopicMcuTemp = "Mcu/Temp";                           // Temp_
constexpr const char* kTopicMcuInit = "Mcu/Init";                           // McuInit_
constexpr const char* kTopicMcuFault = "Mcu/Fault";                         // Fault_

// 运控命令。
constexpr const char* kTopicEnableMotors = "rt/enable/motors";              // EnableMotors_
constexpr const char* kTopicClearErrors = "rt/clear/errors";                // ClearErrors_
constexpr const char* kTopicGoHome = "rt/go/home";                          // GoHome_
constexpr const char* kTopicSetCalibrationFlag = "rt/set/calibration/flag"; // SetCalibrationFlag_
constexpr const char* kTopicSetFsmId = "rt/set/fsm/id";                     // SetFsmId_
constexpr const char* kTopicRemoteControl = "rt/remote/control";            // RemoteControl_

// 状态检测。
constexpr const char* kTopicDiagnosticReport = "rt/diagnostic/report";      // DiagnosticReport_
constexpr const char* kTopicLocomotionAlarm = "rt/locomotion/alarm/state";  // LocomotionAlarmState_
constexpr const char* kTopicWifiStatus = "rt/wifi/status";                  // WifiStatus_
constexpr const char* kTopicPerfFault = "dobot_perf_fault";                 // FaultInfo_

// 语音。
constexpr const char* kTopicVoiceCommand = "rt/voice/command";              // VoiceCommand_
constexpr const char* kTopicVoiceTtsRequest = "rt/voice/tts_request";       // TTSRequest_
constexpr const char* kTopicVoiceAckRequest = "rt/voice/ack_request";       // AckRequest_
constexpr const char* kTopicVoiceOutputEvent = "rt/voice/output_event";     // VoiceOutputEvent_

// 头部。
constexpr const char* kTopicHeadEyeEffectCmd = "rt/head/eye_effect_cmd";    // HeadEyeEffectCmd_
constexpr const char* kTopicHeadInfoRequest = "rt/head/info_request";       // HeadInfoRequest_
constexpr const char* kTopicHeadState = "rt/head/state";                    // HeadState_
constexpr const char* kTopicHeadTouchEvent = "rt/head/touch_event";         // HeadTouchEvent_
constexpr const char* kTopicHeadErrorEvent = "rt/head/error_event";         // HeadErrorEvent_

// Session::Init 使用的连接描述。话题名仍用上面的常量。
inline dobot::sdk::RobotProfile Profile() {
    return {kModel, kDefaultRobotIp, kAlgsRpcPort};
}

}  // namespace dobot::robot::sr_3588
