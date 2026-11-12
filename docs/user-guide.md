**English** | [简体中文](./user-guide.zh-CN.md)

# dobot-sdk User Guide

This guide is for developers building on top of the `dobot-sdk` directory. The directory contains headers, prebuilt libraries, and prebuilt examples. You link against it with C++17; the SDK implementation is not compiled by you.

The SDK provides two channels:

*   **RPC** runs over TCP, one call per operation: query the state machine (FSM), switch the FSM, set velocity, load skills, play actions.
    
*   **DDS** is for receiving state continuously, or for sending commands at your own rate. Both subscribing and publishing go through `dobot::sdk::Session`.
    

The two robots share the same RPC methods and DDS topics. They differ in namespace, default IP, and each has its own set of topic constants.

| Model | Namespace | Topic constants header | Default IP | RPC port | DDS domain |
| --- | --- | --- | --- | --- | --- |
| SR\_3588 | `dobot::robot::sr_3588` | `include/dobot/robot/sr_3588/topics.hpp` | `192.168.5.2` | `51234` | `0` |
| SR\_thor | `dobot::robot::sr_thor` | `include/dobot/robot/sr_thor/topics.hpp` | `172.16.20.2` | `51234` | `0` |

The examples below use SR\_3588. For SR\_thor, replace `dobot::robot::sr_3588` with `dobot::robot::sr_thor` and use IP `172.16.20.2`. Topic strings do not change.

You need C++17 and CMake 3.16 or later. The prebuilt libraries require GLIBC 2.34 and GLIBCXX 3.4.30 or later; Ubuntu 22.04 qualifies. `lib/aarch64`, `lib/x86_64`, and the directories of the same names under `thirdparty/lib/` each hold one set of libraries; CMake selects one according to the target architecture of the current compiler.

All paths below are relative to the `dobot-sdk` directory.

## 0. Network connectivity

RPC and DDS are two independent paths. A successful `ping` only shows that ICMP reaches the robot; it does not replace the two checks below.

| Channel | Address used | Transport | Symptom when it fails |
| --- | --- | --- | --- |
| RPC | `SessionConfig.robot_ip`; if empty, the default IP in the table above | TCP, port `51234` | a `connect` or `no_response` error |
| DDS | No robot IP. Depends on the domain and network interface | UDP from a PC; shared memory is also possible when the program runs on the robot itself | `matched` stays 0 and `Latest()` has no data |

### 0.1 PC to robot

The PC and the robot must be on the same subnet, with no router between them that drops UDP multicast. DDS discovery uses multicast `239.255.0.1`. `SessionConfig` currently has no field for a list of peer IPs, so when multicast is blocked, RPC may work normally while the DDS match count stays 0.

With domain 0, Fast DDS ports start at 7400: multicast metatraffic on 7400, unicast metatraffic from 7410, and user data from 7411. As participants are added, ports increase. The firewall must allow **TCP 51234** and **UDP 7400–7500** on the network interface connected to the robot. If only TCP is allowed, `GetFsmId` succeeds but state subscriptions receive no data.

With several network interfaces (wired, Wi-Fi, and Docker virtual interfaces at the same time), specify the one connected to the robot. List interface names with:

```bash
ip -br link

```

Use an interface name such as `eth0` or `enp3s0`.

Verify in this order, running from the `dobot-sdk` directory. On an x86\_64 PC use `example/bin/x86_64/`; on the robot itself use `example/bin/aarch64/`.

1.  Check that the default address is reachable.
    
    ```bash
    ping -c 3 192.168.5.2
    
    ```
    
    If it is not, use the robot's actual address and add `--ip` to every command below.
    
2.  Check the RPC port.
    
    ```bash
    timeout 2 bash -c 'cat < /dev/null > /dev/tcp/192.168.5.2/51234' && echo "port 51234 reachable"
    
    ```
    
3.  Query the current FSM once. On success it prints the ID and a short name; `0` is a valid ID.
    
    ```bash
    ./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2
    
    ```
    
4.  Subscribe to lower-body and upper-body state. On success you first see `matched lower=` and `upper=` greater than 0, then the joint angles are printed.
    

```bash
./example/bin/x86_64/sr_3588_dds_echo --nic eth0

```

Add `--domain` if the domain is not 0. With only one network interface you can omit `--nic`. The default wait is 5 seconds; change it with `--seconds`.

If `example/bin/` for an architecture is empty, that set has not been shipped; use an architecture that is present.

### 0.2 Program running on the robot itself

RPC connects to `127.0.0.1:51234`, and DDS discovers other processes on the same machine. Add `--on-robot` to the prebuilt programs:

```bash
./example/bin/aarch64/sr_3588_motion_demo --on-robot
./example/bin/aarch64/sr_3588_dds_echo --on-robot

```

In your own code, set `config.where` to `dobot::sdk::RunWhere::kRobot` and do not set `robot_ip`. If you set `robot_ip`, the SDK connects to that address and no longer switches to `127.0.0.1` automatically.

### 0.3 Connection parameters in code

All later examples start from here. `robot_ip` is used only by RPC. `network_interface` is used only by DDS; leaving it empty means all network interfaces take part in discovery. `rpc_port` of 0 means the model's default port 51234. `async_subscribe` defaults to false: subscription callbacks run on the DDS listener thread. When set to true, callbacks move to an ordered worker. For periodic state, read `Latest()`; you do not need to change this switch for callbacks.

```cpp
dobot::sdk::SessionConfig config;
config.where = dobot::sdk::RunWhere::kComputer;  // use kRobot when running on the robot
config.robot_ip = "192.168.5.2";                  // if empty, the model's default IP is used
config.domain_id = 0;
config.network_interface = "eth0";                // may be left empty with a single interface
config.async_subscribe = false;                   // default; when true, callbacks move to an ordered worker

```
```cpp
dobot::sdk::Session session;
std::string error;
if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
    std::cerr << error << "\n";
    return 1;
}

```

`Init` opens RPC and DDS together. Calling `Init` again returns false; call `Shutdown` first. `Shutdown` waits for running callbacks to finish and then releases this session; it does not require the robot to stop. One `Session` connects to one robot. To connect to two robots, create two `Session` objects.

`Init` writes a DDS configuration for this run to the system temporary directory and deletes it after the session is established. If the temporary directory is not writable, `Init` fails.

## 1. Subscribing to a topic without a callback

Read `Latest()` for periodic state in your own thread. When there is no data yet it returns empty; a missing value is never turned into a numeric 0. `WaitForData` waits for the first frame or a timeout; if data is already present it returns true immediately. `Matched()` is the number of peers currently matched; 0 means no publisher has been discovered yet.

Only one topic can be subscribed per `PubSubType`. After subscribing to `rt/pelvis/imu/data`, subscribing to `rt/torso/imu/data` fails, because both use `IMUState_`. When `Subscribe` fails, `valid()` is false.

The example below subscribes to lower-body and upper-body state, waits until both have data, and prints the first 4 joints of each.

```cpp
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "lower_state.hpp"
#include "lower_statePubSubTypes.hpp"
#include "upper_state.hpp"
#include "upper_statePubSubTypes.hpp"

#include <chrono>
#include <iostream>

int main() {
    dobot::sdk::SessionConfig config;
    config.robot_ip = "192.168.5.2";
    config.network_interface = "eth0";

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    namespace dds = dobot::msg::dds_;
    auto lower = session.Subscribe<dds::LowerState_PubSubType>(dobot::robot::sr_3588::kTopicLowerState);
    auto upper = session.Subscribe<dds::UpperState_PubSubType>(dobot::robot::sr_3588::kTopicUpperState);
    if (!lower.valid() || !upper.valid()) {
        std::cerr << "Failed to create subscriber\n";
        return 1;
    }

    const bool got_lower = lower.WaitForData(std::chrono::seconds(5));
    const bool got_upper = upper.WaitForData(std::chrono::seconds(5));
    std::cout << "matched lower=" << lower.Matched() << " upper=" << upper.Matched() << "\n";
    if (!got_lower && !got_upper) {
        std::cerr << "No state received. Check the domain, and the network_interface if there are several.\n";
        return 1;
    }

    if (const auto sample = lower.Latest()) {
        std::cout << "lower joints=" << sample->motor_state().size() << "\n";
        const size_t n = std::min<size_t>(sample->motor_state().size(), 4);
        for (size_t i = 0; i < n; ++i) {
            const auto& motor = sample->motor_state()[i];
            std::cout << "  [" << i << "] q=" << motor.q() << " dq=" << motor.dq()
                      << " tau_est=" << motor.tau_est() << "\n";
        }
    }
    if (const auto sample = upper.Latest()) {
        std::string robot_type;
        for (char ch : sample->robot_type()) {
            if (ch == '\0') {
                break;
            }
            robot_type.push_back(ch);
        }
        std::cout << "upper robot_type=" << robot_type
                  << " is_upper_control=" << std::boolalpha << sample->is_upper_control() << "\n";
    }
    return 0;
}

```

The template parameter is the `PubSubType`, for example `LowerState_PubSubType`, and the return value is `Subscriber<LowerState_>`. Message types are in the namespace `dobot::msg::dds_`. The header search path already includes `include/dobot/idl`, so include them by file name.

`robot_type` is a character array of length 16; stop reading at `'\0'`. Calling `Latest()` repeatedly in a control loop returns the newest frame at that moment.

Matching source in the package: `example/src/sr_3588/dds_echo.cpp`.

## 2. Subscribing to a topic with a callback

`On()` is for events such as e-stop, faults, and touch. For each incoming frame, the SDK first updates `Latest()` and then calls the callback. When `async_subscribe` is false, the callback runs on the Fast DDS listener thread immediately after the sample is taken. When it is true, the callback runs on a worker of an ordered queue.

Keep what the callback does short:

*   You may read the frame, or do an atomic count and hand the data to your own thread.
    
*   Do not call `Shutdown`, do not `Subscribe` / `Publish` again, do not call RPC, and do not sleep or do long computations inside it.
    
*   An exception thrown by the callback only stops that one invocation; later frames keep arriving.
    

For periodic data such as joint angles, use `Latest()` from section 1. If you only want to measure frame rate, have the callback only increment a counter and print from the main thread. `example/src/sr_3588/upper_state_rate.cpp` in the package is written this way.

The example below subscribes to `Mcu/Estop`. `stop1` and `stop2` equal to 0 mean e-stop; 1 means normal.

```cpp
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "mcu_data.hpp"
#include "mcu_dataPubSubTypes.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    dobot::sdk::SessionConfig config;
    config.robot_ip = "192.168.5.2";
    config.network_interface = "eth0";

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    namespace dds = dobot::msg::dds_;
    auto estop = session.Subscribe<dds::Estop_PubSubType>(dobot::robot::sr_3588::kTopicMcuEstop);
    if (!estop.valid()) {
        std::cerr << "Failed to create subscriber\n";
        return 1;
    }

    std::atomic<int> callbacks{0};
    estop.On([&callbacks](const dds::Estop_& msg) {
        const int n = ++callbacks;
        std::cout << "Callback #" << n
                  << " stop1=" << static_cast<unsigned>(msg.stop1())
                  << " stop2=" << static_cast<unsigned>(msg.stop2()) << "\n";
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && callbacks.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "callbacks=" << callbacks.load() << " matched=" << estop.Matched() << "\n";
    if (callbacks.load() == 0) {
        std::cerr << "No e-stop topic received. Check the domain and the network interface.\n";
        return 1;
    }
    return 0;
}

```

`On()` can be set after `Subscribe`. Frames that arrived before it was set remain in `Latest()` but are not replayed into the callback. A copied `Subscriber` refers to the same subscription.

Matching source in the package: `example/src/sr_3588/dds_event.cpp`.

## 3. Publishing data

The template parameter of `Publish` is likewise the `PubSubType`. Create one publisher per topic and call `Write` repeatedly in the control loop. A new frame overwrites the old one; commands are not queued.

`Write` returning true only means the frame was handed to the local DDS; it does not mean the robot has executed it. Check that `Matched()` is greater than 0 before writing.

Joint commands act on the motors. Before sending, confirm the robot is in a state that executes that topic. The example below first subscribes to `rt/upper/state`, then writes the angles it has just read back unchanged to `rt/upper/cmd`, to demonstrate publishing. It does not plan a new trajectory. If no state has been read, or the publisher has no match, the example does not send.

`mode` is 11, `kp` is 10, and `kd` is 1, consistent with the `upper_arm_swing` example in the package. The IDL does not give a table of `mode` values. To actually move joints, first read `example/src/sr_3588/upper_arm_swing.cpp`: it requires the robot to already be in an enabled state that executes `rt/upper/cmd`, and it sends continuously only with `--move`.

```cpp
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "upper_cmd.hpp"
#include "upper_cmdPubSubTypes.hpp"
#include "upper_state.hpp"
#include "upper_statePubSubTypes.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    dobot::sdk::SessionConfig config;
    config.robot_ip = "192.168.5.2";
    config.network_interface = "eth0";

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    namespace dds = dobot::msg::dds_;
    auto upper = session.Subscribe<dds::UpperState_PubSubType>(dobot::robot::sr_3588::kTopicUpperState);
    auto writer = session.Publish<dds::UpperCmd_PubSubType>(dobot::robot::sr_3588::kTopicUpperCmd);
    if (!upper.valid() || !writer.valid()) {
        std::cerr << "Failed to create subscriber or publisher\n";
        return 1;
    }

    if (!upper.WaitForData(std::chrono::seconds(3))) {
        std::cerr << "No upper-body state; not sending a command\n";
        return 1;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && writer.Matched() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (writer.Matched() == 0) {
        std::cerr << "rt/upper/cmd did not match the robot\n";
        return 1;
    }

    const auto sample = upper.Latest();
    dds::UpperCmd_ cmd;
    const size_t n = std::min(sample->motor_state().size(), cmd.motor_cmd().size());
    for (size_t i = 0; i < n; ++i) {
        auto& motor = cmd.motor_cmd()[i];
        motor.mode(11);
        motor.q(sample->motor_state()[i].q());
        motor.dq(0.0f);
        motor.tau(0.0f);
        motor.kp(10.0f);
        motor.kd(1.0f);
    }
    if (!writer.Write(cmd)) {
        std::cerr << "Write failed\n";
        return 1;
    }
    std::cout << "Wrote back the current upper-body angles, matched=" << writer.Matched() << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}

```

In a control loop, keep this `writer` and call `Write` after updating `q` each cycle. After the process exits, no new joint frames are sent. If you are also sending velocity over RPC, call `session.motion().SetVelocity(0, 0, 0)` before exiting.

The package's `sr_3588_dds_publish_cmd` sends one all-zero frame to `rt/lower/cmd`, and only when `--send` is given. That is a communication match test; in everyday publishing, do not start from an all-zero command.

## 4. Topics you can publish and subscribe

Directions are written from the caller's point of view: **Subscribe** means the robot publishes and the caller reads; **Publish** means the caller publishes and the robot receives. Constant names are the same on both models.

All message types are in `dobot::msg::dds_`. When subscribing or publishing, append `_PubSubType` to the type name. The generated accessors have the same name as the field: for example `motor.q()` reads and `motor.q(value)` writes.

QoS is compiled into the library and selected by "topic name.reader" and "topic name.writer"; the caller does not fill it in. For topics that have an entry in the library, reliability and history depth are as follows. A topic with no entry in the library can still be created; the log shows `QoS profile ... not found`, and the Fast DDS default QoS is used (RELIABLE, KEEP\_LAST, depth 1). If the peer is BEST\_EFFORT, such a default reader or writer will not match, and `Matched()` stays 0.

History is always KEEP\_LAST and durability VOLATILE. The depth in the "QoS" column below is the number of samples kept.

| Direction | Topic | Constant | Type | QoS | Description |
| --- | --- | --- | --- | --- | --- |
| Subscribe | `rt/lower/state` | `kTopicLowerState` | `LowerState_` | BEST\_EFFORT, depth 2 | Lower-body joint feedback, 15 channels |
| Subscribe | `rt/upper/state` | `kTopicUpperState` | `UpperState_` | BEST\_EFFORT, depth 2 | Upper-body joint feedback, 18 channels, with robot type name and whether the upper body is under control |
| Publish | `rt/lower/cmd` | `kTopicLowerCmd` | `LowerCmd_` | BEST\_EFFORT, depth 2 | Lower-body joint commands, 12 channels |
| Publish | `rt/upper/cmd` | `kTopicUpperCmd` | `UpperCmd_` | BEST\_EFFORT, depth 2 | Upper-body joint commands, 18 channels |
| Publish | `rt/stand/cmd` | `kTopicStandCmd` | `StandingControl_` | No separate entry in the library | Head yaw, waist yaw, and body height in Mini stand (FSM 300) |
| Subscribe | `rt/pelvis/imu/data` | `kTopicPelvisImu` | `IMUState_` | BEST\_EFFORT, depth 4 | Pelvis IMU, `imu_id` is 1 |
| Subscribe | `rt/torso/imu/data` | `kTopicTorsoImu` | `IMUState_` | BEST\_EFFORT, depth 4 | Torso IMU, `imu_id` is 2. Cannot be subscribed together with the pelvis IMU |
| Subscribe | `rt/gamepad/state` | `kTopicGamepadState` | `GamepadState_` | BEST\_EFFORT, depth 4 | Gamepad buttons and sticks |
| Subscribe | `rt/main/nodes/state` | `kTopicMainNodesState` | `MainNodesState_` | BEST\_EFFORT, depth 2 | Servo nodes and EtherCAT slaves |
| Subscribe | `Mcu/BmsCommon` | `kTopicMcuBmsCommon` | `BmsCommon_` | RELIABLE, depth 1 | Common battery telemetry; for charge level see `remaining_pct` |
| Subscribe | `Mcu/BmsUncommon` | `kTopicMcuBmsUncommon` | `BmsUncommon_` | RELIABLE, depth 1 | Per-cell voltage, cell temperature, version |
| Subscribe | `Mcu/Heartbeat` | `kTopicMcuHeartbeat` | `Heartbeat_` | No separate entry in the library | MCU heartbeat sequence number |
| Subscribe | `Mcu/Estop` | `kTopicMcuEstop` | `Estop_` | RELIABLE, depth 1 | Two e-stop channels. 0 is e-stop, 1 is normal |
| Subscribe | `Mcu/Adc` | `kTopicMcuAdc` | `Adc_` | No separate entry in the library | Raw MCU power-supply ADC codes |
| Subscribe | `Mcu/Temp` | `kTopicMcuTemp` | `Temp_` | No separate entry in the library | Board temperature, 0.01°C per unit |
| Subscribe | `Mcu/Init` | `kTopicMcuInit` | `McuInit_` | RELIABLE, depth 1 | MCU power-on information: network, IP, uptime, firmware version |
| Subscribe | `Mcu/Fault` | `kTopicMcuFault` | `Fault_` | No separate entry in the library | MCU fault codes that have not yet been cleared |
| Publish | `rt/enable/motors` | `kTopicEnableMotors` | `EnableMotors_` | RELIABLE, depth 1 | Enable or disable per joint, `flag[30]` |
| Publish | `rt/clear/errors` | `kTopicClearErrors` | `ClearErrors_` | RELIABLE, depth 1 | Request to clear driver errors |
| Publish | `rt/go/home` | `kTopicGoHome` | `GoHome_` | RELIABLE, depth 1 | Return to zero per joint, `flag[30]` |
| Publish | `rt/set/calibration/flag` | `kTopicSetCalibrationFlag` | `SetCalibrationFlag_` | RELIABLE, depth 1 | Set the zero-calibration flag per joint, `flag[30]` |
| Subscribe | `rt/set/fsm/id` | `kTopicSetFsmId` | `SetFsmId_` | RELIABLE, depth 1 | Published by the locomotion algorithm. Empty or `"\0"` means not playable, `"enable"` means playable but nothing playing, any other string is the name of the skill being played. To change the FSM, use `session.motion()` |
| Publish | `rt/remote/control` | `kTopicRemoteControl` | `RemoteControl_` | BEST\_EFFORT, depth 2 | Remote-control sticks. Left stick translates, right stick turns |
| Subscribe | `rt/diagnostic/report` | `kTopicDiagnosticReport` | `DiagnosticReport_` | RELIABLE, depth 1 | Aggregated diagnostics for motors, IMU, gamepad, performance, locomotion, head, lights, network, serial ports, and the main node |
| Subscribe | `rt/locomotion/alarm/state` | `kTopicLocomotionAlarm` | `LocomotionAlarmState_` | No separate entry in the library | Locomotion alarms that have not yet been cleared |
| Subscribe | `rt/wifi/status` | `kTopicWifiStatus` | `WifiStatus_` | No separate entry in the library | Wi-Fi provisioning heartbeat, mode, IP, faults |
| Subscribe | `dobot_perf_fault` | `kTopicPerfFault` | `FaultInfo_` | No separate entry in the library | Performance-monitoring fault or recovery. The topic name has no `rt/` prefix |
| Subscribe | `rt/voice/command` | `kTopicVoiceCommand` | `VoiceCommand_` | No separate entry in the library | Speech recognition result |
| Publish | `rt/voice/tts_request` | `kTopicVoiceTtsRequest` | `TTSRequest_` | No separate entry in the library | Request to synthesize and speak text |
| Publish | `rt/voice/ack_request` | `kTopicVoiceAckRequest` | `AckRequest_` | No separate entry in the library | Request to play an existing fixed audio clip |
| Subscribe | `rt/voice/output_event` | `kTopicVoiceOutputEvent` | `VoiceOutputEvent_` | No separate entry in the library | Whether a playback request was accepted, and start, end, interruption, failure |
| Publish | `rt/head/eye_effect_cmd` | `kTopicHeadEyeEffectCmd` | `HeadEyeEffectCmd_` | RELIABLE, depth 1 | Set the eye expression |
| Publish | `rt/head/info_request` | `kTopicHeadInfoRequest` | `HeadInfoRequest_` | RELIABLE, depth 1 | Ask the head for device information once |
| Subscribe | `rt/head/state` | `kTopicHeadState` | `HeadState_` | BEST\_EFFORT, depth 2 | The head's reply to an information request; not periodic state |
| Subscribe | `rt/head/touch_event` | `kTopicHeadTouchEvent` | `HeadTouchEvent_` | BEST\_EFFORT, depth 2 | Head touch |
| Subscribe | `rt/head/error_event` | `kTopicHeadErrorEvent` | `HeadErrorEvent_` | BEST\_EFFORT, depth 2 | Head errors |

`include/dobot/idl/` contains other generated types as well. For types that do not appear in `topics.hpp`, this SDK provides no topic name; do not guess topic strings.

Mapping between types and headers:

| Type | Header |
| --- | --- |
| `LowerState_` | `lower_state.hpp` |
| `UpperState_` | `upper_state.hpp` |
| `LowerCmd_` | `lower_cmd.hpp` |
| `UpperCmd_` | `upper_cmd.hpp` |
| `StandingControl_` | `standing_control.hpp` |
| `IMUState_` | `imu_state.hpp` |
| `GamepadState_` | `gamepad_state.hpp` |
| `MainNodesState_` | `main_nodes_state.hpp` |
| `BmsCommon_`, `BmsUncommon_`, `Heartbeat_`, `Estop_`, `Adc_`, `Temp_`, `McuInit_`, `Fault_` | `mcu_data.hpp` |
| `EnableMotors_` | `enable_motors.hpp` |
| `ClearErrors_` | `clear_errors.hpp` |
| `GoHome_` | `go_home.hpp` |
| `SetCalibrationFlag_` | `set_calibration_flag.hpp` |
| `SetFsmId_` | `set_fsm_id.hpp` |
| `RemoteControl_` | `remote_control.hpp` |
| `DiagnosticReport_` | `diagnostic_service.hpp` |
| `LocomotionAlarmState_` | `locomotion_alarm_state.hpp` |
| `WifiStatus_` | `wifi_status.hpp` |
| `FaultInfo_` | `dobot_perf_risk.hpp` |
| `VoiceCommand_`, `TTSRequest_`, `AckRequest_`, `VoiceOutputEvent_` | `voice.hpp` |
| `HeadEyeEffectCmd_`, `HeadInfoRequest_`, `HeadState_`, `HeadTouchEvent_`, `HeadErrorEvent_` | `head_control.hpp` |

Next to each header there is a `PubSubTypes.hpp` file of the same base name; include both. For the field-by-field structure, see the [DDS topics reference](./dds-topics.md) in the same directory. What follows covers only the conventions you must know when calling.

### 4.1 Joint state and commands

`MotorState_` is used for feedback: `mode`, `q`, `dq`, `ddq`, `tau_est`, `tau_cmd`, current, raw values, three temperatures, `bus_voltage`. `timestamp_us` is on `LowerState_` and `UpperState_`, in microseconds. `MotorCmd_` is used for commands: `mode`, `q`, `dq`, `tau`, `kp`, `kd`.

The IDL does not state units for `q`, `dq`, torque, or temperature. The upper-body arm-swing example in the package fills `q` in radians.

Index conventions:

| Data | Length | Indices |
| --- | --- | --- |
| `rt/lower/cmd` | 12 | 0–5 left leg, 6–11 right leg |
| `rt/lower/state` | 15 | The first 12 align with the lower-body command. The meaning of the extra 3 is not specified |
| `rt/upper/state` and `rt/upper/cmd` | 18 | 0–2 waist, 3–9 left arm, 10–16 right arm, 17 head |

The IDL does not define a joint name and positive direction for each channel. The arm-swing example treats upper-body indices 4 and 11 as the channel in which the left and right arms swing outward.

`UpperState_.is_upper_control` being true means the upper body is currently under control. `robot_type` is a character array of length 16; its table of values is not listed in the IDL.

The three floats of `rt/stand/cmd` take effect together: `head_yaw` is the absolute head yaw target in rad; `waist_yaw` is the waist yaw offset relative to the default posture in rad; `body_height` is the body height target in m, and 0 restores the default standing height. Waist pitch and roll are not handled here. Before sending, enter Mini stand with `session.motion().SetRobotMode(dobot::motion::RobotMode::kStanding)`. This topic has no separate QoS entry in the library; if the match count is 0, first check the log for the profile warning.

### 4.2 IMU, gamepad, main nodes

The two IMUs have the same fields: `quaternion[4]`, `gyroscope[3]`, `accelerometer[3]`, `rpy[3]` (roll, pitch, yaw), `temperature`, `timestamp_us`. The quaternion order and physical units are not stated in the IDL. A session can subscribe to only one of the two. The pelvis IMU is published periodically; for the torso IMU the publishing function exists but the current onboard main loop does not call it.

Bits of the gamepad `btn_state`:

| bit | Key | bit | Key |
| --- | --- | --- | --- |
| 0 | R1 | 8 | A |
| 1 | L1 | 9 | B |
| 2 | start | 10 | X |
| 3 | select | 11 | Y |
| 4 | R2 | 12 | up |
| 5 | L2 | 13 | right |
| 6 | F1 | 14 | down |
| 7 | F2 | 15 | left |

`lx`, `ly`, `rx`, `ry` are the left and right sticks. The value range is not stated in the IDL.

`MainNodesState_` contains left leg 6, right leg 6, waist 3, left arm 7, right arm 7, head 2, and two EtherCAT slaves. Each axis has servo state, error code, warning code, temperature, bus voltage, software version, and zero-calibration result. `zero_calib`: 0 not successful, 1 successful, 2 unknown.

### 4.3 Battery and MCU

The charge percentage is `BmsCommon_.remaining_pct`, 0–100. `current_ma` is in mA and negative while charging. Cell and MOS temperature fields ending in `_x10` are in 0.1°C per unit. Among the voltage fields, `max_cell_mv` and `min_cell_mv` are in mV; those with `_10mv` are in 10 mV per unit.

`local_temp` and `remote_temp` of `Mcu/Temp` are degrees Celsius multiplied by 100. `Mcu/Adc` carries raw ADC codes; the conversion is not in the IDL. For the number of valid fault codes in `Mcu/Fault`, see `dtc_count`; the list capacity is 32. `network_ok` of `Mcu/Init` equal to 1 means the MCU network is up; `version` is at most 32 characters.

### 4.4 Locomotion command topics

`EnableMotors_`, `GoHome_`, and `SetCalibrationFlag_` are all `flag[30]`. The IDL does not specify how the 30 channels map to specific joints.

`ClearErrors_.msg_id` identifies this particular request.

`rt/set/fsm/id` is published by the locomotion algorithm and subscribed by the caller. `SetFsmId_` carries `id` and `current_action`. `current_action` empty or `"\0"` means not playable, `"enable"` means playable but nothing is playing, and any other non-empty string is the name of the skill being played. To change the FSM use `session.motion()` from section 5; do not write to this topic. `example/src/sr_3588/action_watch.cpp` in the package subscribes to it to observe skill start and end.

In `RemoteControl_`, `btn_move` is the left stick and `btn_turn` is the right stick, each with `x` and `y`. The stick range and positive direction are not stated in the IDL. Velocity source priority is: gamepad over App, and App over `SetVelocity`. A lower-priority source must wait until the higher-priority one has sent no new command for 0.5 seconds.

### 4.5 Diagnostics, alarms, Wi-Fi, performance

Each block of `DiagnosticReport_` has its own `data_valid`. When it is false, do not use that block. There are many fields; treat `diagnostic_service.hpp` as authoritative.

`LocomotionAlarmState_.active_alarms` holds at most 256 entries. `truncated` being true means the list was truncated. Severity: 1 warning, 2 error, 3 critical. Source: 1 axis, 2 e-stop, 3 IMU, 4 stand-up, 5 lift, 6 lower-body command, 7 upper-body command, 8 hand command, 9 startup, 0 unknown.

`WifiStatus_.event_type`: 0 heartbeat, 1 provisioning started, 2 succeeded, 3 failed, 4 IP changed, 5 fault, 6 exited. `work_mode`: 0 idle, 1 station, 2 ap. `seq` increases monotonically.

`FaultInfo_.is_recovered` being true means this is a recovery. `fault_level`: 0 to 3 correspond to S0 to S3.

### 4.6 Voice

All four messages carry a `header`: `source` is the name of the originating module, and `timestamp` is Unix time in milliseconds.

For the recognition result `VoiceCommand_`: `is_final` distinguishes intermediate from final results, `confidence` is 0.0–1.0, and `language` is `auto` when it cannot be determined. `command_id` identifies this recognition.

For the playback request `TTSRequest_`: `speak_id` uses a UUID generated by the caller, and `text` is the text to synthesize. Long text is split into fragments by `seq_num`, starting from 0, and `is_final` is true on the last fragment. When `interrupt` is true, the current playback is interrupted. When replying to a particular recognition, put its `command_id` in `reply_to_command_id`. Priority from low to high: `PRIORITY_NORMAL`, `PRIORITY_IMPORTANT`, `PRIORITY_SYSTEM`, `PRIORITY_EMERGENCY`.

`AckRequest_` plays an existing fixed audio clip. `ack_id` is the audio asset ID; do not pass a file path or text.

Results arrive on `rt/voice/output_event`. Branch your program on `reason`; use `detail` only for logs. When `has_seq_num` is false, ignore `seq_num`; ACK and whole-playback lifecycle events fall in this case. `event_type` includes `REQUEST_BUFFERED`, `REQUEST_ACCEPTED`, `REQUEST_REJECTED`, `REQUEST_DROPPED`, `PLAYBACK_STARTED`, `PLAYBACK_COMPLETED`, `PLAYBACK_INTERRUPTED`, `PLAYBACK_FAILED`.

### 4.7 Head

`rt/head/state` is not a periodic topic. Publish `rt/head/info_request` first, and the head then replies once on `rt/head/state`. Touch and error are events; use `On()`.

`eye_effect` values:

| Value | Name | Value | Name |
| --- | --- | --- | --- |
| `0x01` | ANGRY | `0x0A` | SERIOUS |
| `0x02` | BASE | `0x0B` | TIRED |
| `0x03` | CAREFUL | `0x0C` | WAKEUP |
| `0x04` | DAZE | `0x0D` | WINK1 |
| `0x05` | DEYI | `0x0E` | YUNTOU |
| `0x06` | GANGA | `0x0F` | ZHENWANG |
| `0x07` | HUAIXIAO | `0x10` | WINK2 |
| `0x08` | JIANDING | `0x11` | GLANCE2 |
| `0x09` | MOON | `0xFF` | Use `color` |

`color` takes effect only when `eye_effect` is `0xFF`. `color` is RGB888, with the low 24 bits valid. `duration_ms` of 0 means use the MCU's default hold time. `control_node_id` is for record only; the head does not use it for routing.

Touch zones: `HEAD_TOUCH_TOP`, `HEAD_TOUCH_LEFT_0`, `HEAD_TOUCH_LEFT_1`, `HEAD_TOUCH_RIGHT_0`, `HEAD_TOUCH_RIGHT_1`. `pressed` may contain several zones at once.

## 5. Using the RPC interface

For everyday motion control use `session.motion()`. Calls are blocking: connect, send one JSON-RPC 2.0 request, and return after reading the response or timing out. The default timeout is 3000 ms and covers connect, send, and receive. Each call opens a new TCP connection, so the caller does not need to keep a connection alive. On success it returns `0`; otherwise it returns an integer consistent with the error code table below (the same as `RpcError`). `GetFsmId` writes the current FSM ID, and `0` is valid. `GetFsmList` writes a `std::vector<dobot::motion::FsmInfo>`. `FsmIdToString` translates an ID into a short string.

The prebuilt program `sr_3588_motion_demo` queries the current FSM when run without arguments. Add `--mode` to switch mode, `--action` for a preset action, and `--dance` for a dance. When `--fsm-id` maps to one of these interfaces, the corresponding function is used; other IDs go through `session.rpc().SetFsmId`. The velocity example is `sr_3588_walk_demo`, and it requires `--run`.

```cpp
#include "dobot/motion/motion.hpp"
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/rpc_client.hpp"
#include "dobot/sdk/session.hpp"

#include <iostream>

int main() {
    dobot::sdk::SessionConfig config;
    config.robot_ip = "192.168.5.2";

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    const dobot::motion::Motion motion = session.motion();
    int32_t fsm_id = 0;
    if (const int32_t code = motion.GetFsmId(&fsm_id); code != 0) {
        std::cerr << "GetFsmId failed "
                  << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
        return 1;
    }
    std::cout << "Current FSM " << fsm_id << " ("
              << dobot::motion::Motion::FsmIdToString(fsm_id) << ")\n";

    if (const int32_t code = motion.SetRobotMode(dobot::motion::RobotMode::kStanding); code != 0) {
        std::cerr << "SetRobotMode failed "
                  << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
        return 1;
    }

    // Stops when duration expires. Send zero velocity once more when the program exits on its own.
    if (const int32_t code = motion.SetVelocity(0.2f, 0.0f, 0.0f, 1.0f); code != 0) {
        std::cerr << "SetVelocity failed "
                  << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
        motion.SetVelocity(0.0f, 0.0f, 0.0f);
        return 1;
    }
    motion.SetVelocity(0.0f, 0.0f, 0.0f);
    return 0;
}

```

`SetRobotMode` mapping: `kZeroTorque` 0, `kDamping` 1, `kCustomDebug` 2, `kReady` 100, `kStanding` 300, `kWalking` 304. Only after entering Mini stand can you send `rt/stand/cmd`.

Preset actions: after `AutoEnterBox` (307), `LieDown` (310), and `GoProne` (312) complete, locomotion automatically enters damping (1); after `AutoExitBox` (308), `StandUpFromSupine` (311), and `StandUpFromProne` (313) complete, it first enters humanoid walking (304) and about 2 seconds later automatically enters stand (300). Do not switch to stand immediately after `AutoExitBox` returns. Success only means the switch was accepted.

For dances use `PlayDance(fsm_id)`. `fsm_id` must be in 350–400. Do not mix it with the six presets above. Existing dances are read from `GetFsmList`: when `is_dance` is true there are also `dance_name`, `model_file`, `motion_loop_length`, `motion_hold_back_steps`, and `motion_start_step`.

CSV files in the skill directory, reloading skills, and sending key mappings still go through `session.rpc()`. This layer returns `RpcResult`. Check `ok()` first. `ok()` true means the transport succeeded and the response contains `result`. For methods that change robot state, when the robot returns `result` 0, the wrapper function sets `ok()` to false and fills in a business error code. `result()` throws if that field is absent. `error` is turned into a stable short string by `RpcErrorName`, `message` is a description, and `response` is the raw JSON. The timeout of `ExecuteAction` covers only this RPC; being accepted does not mean the action has finished playing.

```cpp
const dobot::sdk::RpcResult action = session.rpc().ExecuteAction("skill.csv");
if (!action.ok()) {
    std::cerr << "ExecuteAction failed " << dobot::sdk::RpcErrorName(action.error)
              << ": " << action.message << "\n";
    return 1;
}

```

For methods not yet wrapped in a function, use `session.Call`. Pass a JSON object as `params`. This is the same path as the prebuilt program `sr_3588_rpc_call --method ... --params ...`. `GetFsmId`, `SetFsmId`, and `SetVel` already have semantic interfaces; do not call them by method name.

```cpp
const dobot::sdk::RpcResult raw = session.Call("MethodName", nlohmann::json::object(), 3000);

```

Error codes:

| Value | Name | Meaning |
| --- | --- | --- |
| 0 | `ok` | Call succeeded |
| 1 | `socket` | Failed to create the socket |
| 2 | `connect` | Connection failed, invalid IP, or connection timed out |
| 3 | `no_response` | Send failed, receive failed, or no response was read |
| 4 | `json` | Response is not valid JSON, or `result` has the wrong type |
| 5 | `unexpected` | The response has no `result`, or only a JSON-RPC error, or exceeds 1MB. The semantic interface also returns this code when the `PlayDance` ID is outside 350–400 |
| 6 | `server_internal` | `result` is -1, or a business failure with no dedicated error code |
| 100 | `fsm_transition_failed` | FSM transition failed |
| 102 | `set_vel_failed` | Setting the velocity command failed |
| 106 | `execute_action_failed` | Action file execution failed |

Codes 101, 103, 104, and 105 remain in the error code table and currently have no corresponding wrapper method.

## 6. Available RPC interfaces

Both robots share `dobot::sdk::MotionRpc`, declared in `include/dobot/sdk/motion_rpc.hpp`. The model headers contain only the IP, port, and topic names, not a method list.

FSM IDs used by `session.motion()`: 0 zero torque, 1 damping, 2 custom debug, 100 ready stand, 300 Mini stand (only after entering it can you send `rt/stand/cmd`), 304 humanoid walking, 307 auto enter box, 308 auto exit box, 310 lie down, 311 stand up from supine, 312 go prone, 313 stand up from prone. 350 to 400 are dances; use `PlayDance`. These IDs are still to be checked against the onboard `GetFsmId`. Read the dance list with `GetFsmList`; the SDK has no `AddDancePolicy`.

| C++ method | JSON method | Parameters | `result` on success | Notes |
| --- | --- | --- | --- | --- |
| `GetFsmId()` | `GetFsmId` | None | Current FSM ID, integer. 0 is also valid | Query. When `result` is -1, error code 6 |
| `SetFsmId(fsm_id)` | `SetFsmId` | `fsm_id`: int32 | 1 | On a failed switch `result` is 0, error code 100. 300 is Mini stand |
| `GetFsmList()` | `GetFsmList` | None | Array of objects | Each item has `fsm_id`, `is_dance`, `name`. When `is_dance` is true there are also `dance_name`, `model_file`, `motion_loop_length`, `motion_hold_back_steps`, `motion_start_step`. If `result` is not an array, error code 4; if it is -1, error code 6 |
| `SetVel(vx, vy, vyaw, duration)` | `SetVel` | Four floats. `duration` defaults to 1.0 s | 1 | `vx` positive forward, m/s; `vy` positive left, m/s; `vyaw` positive counter-clockwise, rad/s. Stops when time is up. To stop, call `SetVel(0, 0, 0)`. On failure, error code 102. Gamepad and App have higher priority than this RPC |
| `ReloadArmSkillsData()` | `ReloadArmSkillsData` | None | 1 | Reloads the skill list. Before calling, put the trajectory CSV files on the robot at `/dobot/algs/policies/humanoid/motion_data/mini/skills_user_defined` |
| `SendActionMapping(mapping)` | `SendActionMapping` | `std::map<std::string, std::string>`, key is the button, value is the skill name | 1 | Overwrites only the keys present this time; it does not merge with the old mapping |
| `ExecuteAction(file_path)` | `ExecuteAction` | File name in the skill directory | 1 means accepted | Does not mean the action has finished. The timeout covers only the RPC. On failure, error code 106 |

The last parameter of every method is `timeout_ms`, default 3000. The semantic interface `session.motion()` returns 0 on success and the same set of error codes on failure. `PlayDance` returns error code 5 when the ID is not in 350–400.

For methods that already have a semantic interface, use `motion_demo` and `walk_demo` on the command line:

```bash
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2 --mode stand
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2 --action lie
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2 --dance 350
./example/bin/x86_64/sr_3588_walk_demo --ip 192.168.5.2 --run --vx 0.2 --seconds 1

```

Use `rpc_call` only for `ReloadArmSkillsData`, `SendActionMapping`, `ExecuteAction`, and methods that do not yet have a member function. `--method` is required. The field names in `--params` match the table above:

```bash
./example/bin/x86_64/sr_3588_rpc_call --ip 192.168.5.2 --method ReloadArmSkillsData
./example/bin/x86_64/sr_3588_rpc_call --ip 192.168.5.2 --method ExecuteAction --params '{"file_path":"skill.csv"}'

```

This SDK does not wrap whole-robot device status, audio, navigation, or GPIO. Methods that appeared in the error code table but are not in the current interface (upper-body control switch, upper-body joints, lift pose, trajectory recording) should not be `Call`ed under those names. To list dances use `GetFsmList`, and to play one use `session.motion().PlayDance`.

## 7. Integrating into your own project

Put the whole `dobot-sdk` directory into your project. The headers, `lib/<arch>/libdobot_sdk.a`, `lib/<arch>/libdobot_log.so`, and `libfastdds.so` and `libfastcdr.so` under `thirdparty/lib/<arch>/` must stay together. Copying only the headers or only the static library leaves libraries missing at link time or run time.

There are two ways to integrate:

| Method | When to use |
| --- | --- |
| `add_subdirectory` | Use this by default. One directory holds both aarch64 and x86\_64, and CMake selects by the current compiler. The `dobot_sdk` target brings in the thread library, Fast DDS, Fast CDR, and `dobot_log` |
| `install`, then `find_package` | Use only on one architecture, and when you want it installed into a single prefix. One install contains only the architecture selected for that run; for two architectures, install into two prefixes |

### 7.1 Adding the directory to your project

Assume your project is next to `dobot-sdk`, or that you have copied the directory to `third_party/dobot-sdk` in your project.

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# When cross-compiling for aarch64, change this to aarch64 and pass the matching compiler.
set(TARGET_ARCH x86_64)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/dobot-sdk)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dobot_sdk)
if(UNIX AND NOT APPLE)
  set_target_properties(my_app PROPERTIES
    BUILD_RPATH "\$ORIGIN/../third_party/dobot-sdk/lib/${TARGET_ARCH};\$ORIGIN/../third_party/dobot-sdk/thirdparty/lib/${TARGET_ARCH}")
  target_link_options(my_app PRIVATE "LINKER:--disable-new-dtags")
endif()

```

Set `BUILD_EXAMPLES` to OFF to avoid building the SDK's own examples. If you do not set it, the examples are built too.

Adjust `RPATH` to the actual relative location of your executable. You can also specify the shared-library directories before running:

```bash
export LD_LIBRARY_PATH=/path/to/dobot-sdk/lib/x86_64:/path/to/dobot-sdk/thirdparty/lib/x86_64

```

If `libdobot_log.so`, `libfastdds.so`, or `libfastcdr.so` is missing, the program reports a missing shared library before it even enters `main`.

Cross-compilation example:

```bash
cmake -S . -B build -DTARGET_ARCH=aarch64 -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
cmake --build build -j

```

If the system's glibc or libstdc++ is older than the versions given earlier, configuration reports that `libfastdds.so` cannot be linked. `Session::Init` starts DDS, so your own program must also be able to link this set of libraries.

### 7.2 Installing, then find\_package

Run this in the `dobot-sdk` directory. It installs only the set matching the current compiler, and flattens it into the prefix's `lib/`, no longer keeping the `aarch64` and `x86_64` subdirectories.

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/dobot_sdk
cmake --build build
cmake --install build

```

Your project:

```cmake
find_package(dobot_sdk REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dobot_sdk)

```

When the prefix is not on CMake's default search path:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/dobot_sdk

```

The installed `dobot_sdk` target links `fastdds`, `fastcdr`, and the thread library. If linking reports undefined symbols from `dobot_log`, also add `lib/libdobot_log.so` from the prefix to `target_link_libraries`. At run time, put `$HOME/dobot_sdk/lib` in `RPATH` or `LD_LIBRARY_PATH`.

Both integration methods add `include/` and `include/dobot/idl/` to the header search path. Include message headers as `"lower_state.hpp"` and SDK headers as `"dobot/sdk/session.hpp"`.

## 8. What to check first when you cannot connect

| Symptom | Check first |
| --- | --- |
| `connect` | IP, port 51234, the route from this machine to that address. `ping` first, then test TCP |
| `no_response` | Whether the port is 51234 and whether the timeout is too short. Add `--timeout-ms` or increase `timeout_ms` |
| `json` | Whether `--params` or the JSON you built is a complete object |
| `unexpected` or `server_internal` | Whether the method name is in the table in section 6. Print `message` and `response.dump()` |
| Error codes 100, 102, 106 | The network works; the robot rejected this business call |
| RPC succeeds, DDS `Matched()` stays 0 | Whether the domain is 0, whether `network_interface` is the one connected to the robot, whether the firewall allows UDP 7400–7500, and whether the network in between allows multicast `239.255.0.1` |
| `QoS profile ... not found` in the log | This topic is marked "No separate entry in the library" in section 4. If the peer is BEST\_EFFORT, the default RELIABLE will not match |
| State is received, but commands have no effect | Whether the current FSM consumes this topic. `rt/stand/cmd` requires `SetRobotMode(kStanding)` first. A successful `Write` only means it was sent locally |
| Missing `libdobot_log.so` or `libfastdds.so` at startup | Whether `LD_LIBRARY_PATH` or `RPATH` includes `lib/<arch>` and `thirdparty/lib/<arch>` |
| Configuration reports that `libfastdds.so` cannot be linked | System GLIBC is below 2.34, or GLIBCXX is below 3.4.30, or the compiler architecture does not match the `lib/` subdirectory |

Source in the package for reference is in `example/src/sr_3588/` and `example/src/sr_thor/`. The CMake of the public package builds `motion_demo`, `walk_demo`, `rpc_call`, `dds_echo`, `dds_publish_cmd`, `upper_state_rate`, and `upper_arm_swing`. The same directory also contains `dds_event.cpp` and `action_watch.cpp`, which the public package's CMake does not build. `action_watch` subscribes to `rt/set/fsm/id`. Use `motion_demo` to query the FSM and switch modes, and `walk_demo` for velocity. Use `rpc_call` only for methods that have no dedicated example yet. For usage, the code in sections 2, 5, and 6 of this document is authoritative.
