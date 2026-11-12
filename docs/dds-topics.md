**English** | [简体中文](./dds-topics.zh-CN.md)

# DDS Topics

This document covers the DDS topics that the SDK currently exposes as topic constants. SR\_3588 and SR\_thor use the same topic names. The constants are in `dobot::robot::sr_3588` and `dobot::robot::sr_thor` respectively. Message types are in `dobot::msg::dds_`.

Callers subscribe with `Session::Subscribe` and publish with `Session::Publish`. Direction is written from the caller's side: subscribe means the robot is the sender, and publish means the caller is the sender. The default domain is 0. QoS is compiled into the library and selected by "topic name.reader" and "topic name.writer". All entries use KEEP\_LAST and VOLATILE history. Lower-body and upper-body state and commands, main node state, and remote control use BEST\_EFFORT with depth 2. Pelvis IMU, torso IMU, and gamepad use BEST\_EFFORT with depth 4. Enable, clear errors, go home, set calibration flag, `rt/set/fsm/id`, e-stop, battery, MCU init, diagnostics, and head eye effect and info requests use RELIABLE with depth 1. Head state, touch, and error use BEST\_EFFORT with depth 2. Topics without an entry in the library use the Fast DDS default QoS (RELIABLE, KEEP\_LAST, depth 1), and they will not match a BEST\_EFFORT peer. For per-topic QoS, see section 4 of the [User Guide](./user-guide.md).

The repository does not record the effective version per topic. The "Effective version" row in the tables below uniformly states that the current source already provides the topic, shared by both models.

The examples cover lower-body / upper-body state, lower-body commands, and the MCU e-stop. `action_watch` subscribes to `rt/set/fsm/id` to show when a skill starts and ends. The names and types of the other topics are already in the SDK, and they are used in the same way.

The IDL mostly does not fix the units of joint angle, angular velocity, torque, current, temperature, and raw IMU values. These units are still to be confirmed in the secondary development documentation. This document only keeps the units that the IDL states explicitly.

| Topic | Access | Type | Description |
| --- | --- | --- | --- |
| `rt/lower/state` | Subscribe | `LowerState_` | Lower-body joint feedback |
| `rt/upper/state` | Subscribe | `UpperState_` | Upper-body joint feedback |
| `rt/lower/cmd` | Publish | `LowerCmd_` | Lower-body joint commands, 12 channels |
| `rt/upper/cmd` | Publish | `UpperCmd_` | Upper-body joint commands, 18 channels |
| `rt/stand/cmd` | Publish | `StandingControl_` | Head, waist, and body height in Mini stand mode |
| `rt/pelvis/imu/data` | Subscribe | `IMUState_` | Pelvis IMU |
| `rt/torso/imu/data` | Subscribe | `IMUState_` | Torso IMU |
| `rt/gamepad/state` | Subscribe | `GamepadState_` | Gamepad buttons and joysticks |
| `rt/main/nodes/state` | Subscribe | `MainNodesState_` | Servo nodes and EtherCAT slaves |
| `Mcu/BmsCommon` | Subscribe | `BmsCommon_` | Common battery telemetry |
| `Mcu/BmsUncommon` | Subscribe | `BmsUncommon_` | Per-cell battery voltage and versions |
| `Mcu/Heartbeat` | Subscribe | `Heartbeat_` | MCU heartbeat |
| `Mcu/Estop` | Subscribe | `Estop_` | Two e-stop pins |
| `Mcu/Adc` | Subscribe | `Adc_` | MCU power-rail ADC |
| `Mcu/Temp` | Subscribe | `Temp_` | Onboard temperature |
| `Mcu/Init` | Subscribe | `McuInit_` | MCU power-up information |
| `Mcu/Fault` | Subscribe | `Fault_` | Active MCU fault codes |
| `rt/enable/motors` | Publish | `EnableMotors_` | Enable or disable per joint |
| `rt/clear/errors` | Publish | `ClearErrors_` | Request to clear drive errors |
| `rt/go/home` | Publish | `GoHome_` | Go home per joint |
| `rt/set/calibration/flag` | Publish | `SetCalibrationFlag_` | Set the calibration flag per joint |
| `rt/set/fsm/id` | Subscribe | `SetFsmId_` | State machine ID and current skill name, published by the motion control algorithm |
| `rt/remote/control` | Publish | `RemoteControl_` | Remote-control joysticks |
| `rt/diagnostic/report` | Subscribe | `DiagnosticReport_` | Aggregated diagnostics |
| `rt/locomotion/alarm/state` | Subscribe | `LocomotionAlarmState_` | Active locomotion alarms |
| `rt/wifi/status` | Subscribe | `WifiStatus_` | Wi-Fi provisioning status |
| `dobot_perf_fault` | Subscribe | `FaultInfo_` | Performance monitoring fault or recovery |
| `rt/voice/command` | Subscribe | `VoiceCommand_` | Speech recognition result |
| `rt/voice/tts_request` | Publish | `TTSRequest_` | Request to synthesize and speak text |
| `rt/voice/ack_request` | Publish | `AckRequest_` | Request to play a fixed audio clip |
| `rt/voice/output_event` | Subscribe | `VoiceOutputEvent_` | Speech output acceptance and playback events |
| `rt/head/eye_effect_cmd` | Publish | `HeadEyeEffectCmd_` | Set the eye effect |
| `rt/head/info_request` | Publish | `HeadInfoRequest_` | Request head device information |
| `rt/head/state` | Subscribe | `HeadState_` | Head device information reply |
| `rt/head/touch_event` | Subscribe | `HeadTouchEvent_` | Head touch |
| `rt/head/error_event` | Subscribe | `HeadErrorEvent_` | Head error |

## Joint State and Commands

### rt/lower/state

| Field | Content |
| --- | --- |
| Topic | `rt/lower/state` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to lower-body joint feedback. The onboard HAL periodically publishes position, velocity, torque, and temperature. |
| Notes | The state array has length 15. The index convention in the secondary development documentation is that the first 12 channels align with `rt/lower/cmd`: 0–5 are the left leg and 6–11 are the right leg. The meaning of the 3 extra channels, the joint name of each channel, and the positive direction are not yet documented. The IDL does not state the units of `q`, `dq`, torque, and temperature. |

Interface structure:

```text
struct LowerState_ {
    MotorState_ motor_state[15];
    uint64 timestamp_us;              // Timestamp, in microseconds
};

struct MotorState_ {
    uint8 mode;                       // Mode
    float q;                          // Angular position
    float dq;                         // Angular velocity
    float ddq;                        // Angular acceleration
    float tau_est;                    // Estimated torque
    float tau_cmd;                    // Commanded torque
    float current_feedback;           // Feedback current
    float current_cmd;                // Commanded current
    float q_raw;                      // Raw angular position
    float dq_raw;                     // Raw angular velocity
    float ddq_raw;                    // Raw angular acceleration
    float tau_raw;                    // Raw torque
    uint8 mcu_temp;                   // Servo control board temperature
    uint8 mos_temp;                   // MOSFET temperature
    uint8 motor_temp;                 // Motor temperature
    uint8 bus_voltage;                // Bus voltage
};

```

### rt/upper/state

| Field | Content |
| --- | --- |
| Topic | `rt/upper/state` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to upper-body joint feedback, which also carries the robot model name and whether the upper body is currently under control. |
| Notes | `robot_type` is a character array of length 16; its value table is not yet confirmed. `is_upper_control` true means the upper body is under control, and false means it is not. `motor_state` has length 18. The index convention in the secondary development documentation is: 0–2 waist, 3–9 left arm, 10–16 right arm, 17 head. The joint name of each channel, the positive direction, and whether the head channel count matches this convention are not yet confirmed. The `MotorState_` fields are the same as in `rt/lower/state`. |

Interface structure:

```text
struct UpperState_ {
    char robot_type[16];              // Robot model name as defined by the product
    boolean is_upper_control;         // true: upper body under control; false: upper body not under control
    MotorState_ motor_state[18];
    uint64 timestamp_us;              // Timestamp, in microseconds
};

```

### rt/lower/cmd

| Field | Content |
| --- | --- |
| Topic | `rt/lower/cmd` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Publish lower-body joint commands. The HAL subscribes to them and sends them to the motors. There are 12 channels. |
| Notes | Index convention: 0–5 left leg, 6–11 right leg. The joint name of each channel and the positive direction are not yet documented. The allowed values of `mode`, and whether an all-zero frame is treated as a real command, are not yet confirmed. The example `dds_publish_cmd` sends one all-zero command frame only when `--send` is given, to confirm that the writer has matched. Before sending joint commands, make sure the state machine is in a mode that allows low-level control and that the writer has matched with the robot. |

Interface structure:

```text
struct LowerCmd_ {
    MotorCmd_ motor_cmd[12];          // 12 motor commands
};

struct MotorCmd_ {
    uint8 mode;                       // Mode
    float q;                          // Position
    float dq;                         // Velocity
    float tau;                        // Torque
    float kp;                         // Proportional gain
    float kd;                         // Derivative gain
};

```

### rt/upper/cmd

| Field | Content |
| --- | --- |
| Topic | `rt/upper/cmd` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Publish upper-body joint commands. The HAL subscribes to them and sends them to the motors. |
| Notes | The array length is 18. The IDL comment says "17"; the array length 18 takes precedence. The index convention is the same as `rt/upper/state`: 0–2 waist, 3–9 left arm, 10–16 right arm, 17 head. The `MotorCmd_` fields are the same as in `rt/lower/cmd`. |

Interface structure:

```text
struct UpperCmd_ {
    MotorCmd_ motor_cmd[18];          // 18 motor commands
};

```

### rt/stand/cmd

| Field | Content |
| --- | --- |
| Topic | `rt/stand/cmd` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | In Mini stand mode (state machine 300), set head yaw, waist yaw, and body height together. |
| Notes | Accepted only in state machine 300. Each frame replaces all three targets at once. Waist pitch and roll are not controlled here. Before sending, enter stand mode with the RPC `SetFsmId(300)`. |

Interface structure:

```text
struct StandingControl_ {
    float head_yaw;                   // Absolute head yaw target, in rad
    float waist_yaw;                  // Waist yaw offset from the default posture, in rad
    float body_height;                // Body height target, in m; 0 restores the default standing height
};

```

## IMU

Both topics use `IMUState_`. Use the topic and `imu_id` to tell the mounting positions apart. The IDL does not state the quaternion order or the units of gyroscope, acceleration, and `rpy`.

### rt/pelvis/imu/data

| Field | Content |
| --- | --- |
| Topic | `rt/pelvis/imu/data` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 4 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the pelvis IMU. |
| Notes | `imu_id` is 1 when published. The current HAL main loop publishes this channel periodically. |

Interface structure:

```text
struct IMUState_ {
    uint32 imu_id;                    // IMU ID. Pelvis is 1
    float quaternion[4];              // Quaternion
    float gyroscope[3];               // Gyroscope
    float accelerometer[3];           // Accelerometer
    float rpy[3];                     // Euler angles: roll, pitch, yaw
    uint8 temperature;                // Temperature
    uint64 timestamp_us;              // Timestamp, in microseconds
};

```

### rt/torso/imu/data

| Field | Content |
| --- | --- |
| Topic | `rt/torso/imu/data` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 4 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the torso IMU. The fields are the same as for the pelvis IMU. |
| Notes | `imu_id` is 2 when published. The publish function exists, but the current HAL main loop does not call it for this channel. |

Interface structure:

```text
struct IMUState_ {
    uint32 imu_id;                    // IMU ID. Torso is 2
    float quaternion[4];
    float gyroscope[3];
    float accelerometer[3];
    float rpy[3];                     // Roll, pitch, yaw
    uint8 temperature;
    uint64 timestamp_us;              // Timestamp, in microseconds
};

```

## Gamepad and Main Nodes

### rt/gamepad/state

| Field | Content |
| --- | --- |
| Topic | `rt/gamepad/state` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 4 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to gamepad buttons and the left and right joysticks. The HAL publishes after it reads the gamepad. |
| Notes | The IDL does not state the joystick value range. |

Interface structure:

```text
struct GamepadState_ {
    uint16 head;                      // Protocol header
    uint16 btn_state;
    // bit0: R1     bit8:  A
    // bit1: L1     bit9:  B
    // bit2: start  bit10: X
    // bit3: select bit11: Y
    // bit4: R2     bit12: up
    // bit5: L2     bit13: right
    // bit6: F1     bit14: down
    // bit7: F2     bit15: left
    float lx;                         // Left joystick x
    float ly;                         // Left joystick y
    float rx;                         // Right joystick x
    float ry;                         // Right joystick y
    uint64 timestamp_us;              // Timestamp, in microseconds
};

```

### rt/main/nodes/state

| Field | Content |
| --- | --- |
| Topic | `rt/main/nodes/state` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the state, alarms, temperature, version, and calibration result of the servo nodes of the legs, waist, arms, and head, and of the EtherCAT slaves. |
| Notes | Hand and power state are commented out in the IDL and are not in the current message. The IDL does not state the units of temperature and bus voltage. |

Interface structure:

```text
struct MainNodesState_ {
    AxisStateInfo_ left_leg[6];
    AxisStateInfo_ right_leg[6];
    AxisStateInfo_ waist[3];
    AxisStateInfo_ left_arm[7];
    AxisStateInfo_ right_arm[7];
    AxisStateInfo_ head[2];
    EcatSlaveInfo_ ecat2can[2];
    uint64 timestamp_us;              // Timestamp, in microseconds
};

struct AxisStateInfo_ {
    uint8 servo_state;
    uint16 error_code;                // Motor alarm error code
    uint16 warn_code;                 // Motor warning code
    int32 pos_err_code;
    int32 vel_err_code;
    int32 torque_err_code;
    uint8 node_state;
    uint8 display_op_mode;
    boolean is_virtual;               // Default false
    uint8 mcu_temp;
    uint8 mos_temp;
    uint8 motor_temp;
    uint8 bus_voltage;
    uint16 software_version;
    uint8 zero_calib;                 // Calibration state: 0 not successful, 1 successful, 2 unknown
};

struct EcatSlaveInfo_ {
    boolean is_virtual;
    uint8 slave_state;
    uint16 error_code;
    uint16 software_version;
};

```

## Battery and MCU

The battery percentage is in `BmsCommon_.remaining_pct`. These topics are published by the MCU and subscribed to by the caller.

### Mcu/BmsCommon

| Field | Content |
| --- | --- |
| Topic | `Mcu/BmsCommon` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to common battery telemetry: charge and discharge, voltage, current, temperature, and remaining charge. |
| Notes | Charging current is negative. |

Interface structure:

```text
struct BmsCommon_ {
    octet protection[8];              // Protection1/2, Fault1, Underlying1/2, Reserved, NTC, Reserved
    boolean charging;                 // Charging state
    boolean discharging;              // Discharging state
    uint16 max_cell_mv;               // Highest cell voltage, in mV
    uint16 min_cell_mv;               // Lowest cell voltage, in mV
    uint16 total_battery_10mv;        // Total battery voltage, 10 mV/LSB
    uint16 battery_output_10mv;       // Battery output voltage, 10 mV/LSB
    uint16 battery_cplus_10mv;        // Battery C+ voltage, 10 mV/LSB
    uint16 max_cell_temp_x10;         // Highest cell temperature, 0.1°C/LSB
    uint16 min_cell_temp_x10;         // Lowest cell temperature, 0.1°C/LSB
    uint16 chg_mos_temp_x10;          // Charge MOSFET temperature, 0.1°C/LSB
    uint16 dchg_mos_temp_x10;         // Discharge MOSFET temperature, 0.1°C/LSB
    uint16 prechg_mos_temp_x10;       // Pre-charge MOSFET temperature, 0.1°C/LSB
    uint16 max_mos_temp_x10;          // Highest MOSFET temperature, 0.1°C/LSB
    int32 current_ma;                 // Real-time current, in mA, negative when charging
    octet remaining_pct;              // Remaining capacity percentage, 0–100
    uint16 remaining_mah;             // Remaining capacity, in mAh
    uint16 full_charge_mah;           // Full-charge capacity, in mAh
    uint16 cycle_count;               // Charge/discharge cycle count
    octet soh_pct;                    // Battery state of health (SOH), 0–100
};

```

### Mcu/BmsUncommon

| Field | Content |
| --- | --- |
| Topic | `Mcu/BmsUncommon` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to per-cell battery voltage, cell temperatures, accumulated energy, and hardware and software versions. |
| Notes | None. |

Interface structure:

```text
struct BmsUncommon_ {
    uint16 cell_mv[20];               // Voltage of 20 cells, in mV
    int16 cell_temp_x10[8];           // 8 cell temperature channels, 0.1°C/LSB
    octet calib_rsvd[8];              // Current calibration parameters / reserved
    uint32 charge_energy_pct;         // Accumulated charge energy, 1% units
    uint32 discharge_energy_pct;      // Accumulated discharge energy, 1% units
    uint16 hw_version;                // BMS hardware version
    uint16 sw_version;                // BMS software version
};

```

### Mcu/Heartbeat

| Field | Content |
| --- | --- |
| Topic | `Mcu/Heartbeat` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the MCU heartbeat. The sequence number increments. |
| Notes | None. |

Interface structure:

```text
struct Heartbeat_ {
    uint16 seq;                       // Heartbeat sequence number, incrementing
};

```

### Mcu/Estop

| Field | Content |
| --- | --- |
| Topic | `Mcu/Estop` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the two e-stop pins. |
| Notes | 0 is e-stop, and 1 is normal. The example `dds_event` subscribes to this topic and enters the callback when a frame is received. |

Interface structure:

```text
struct Estop_ {
    octet stop1;                      // MCU_STOP1: 0 e-stop, 1 normal
    octet stop2;                      // MCU_STOP2: 0 e-stop, 1 normal
};

```

### Mcu/Adc

| Field | Content |
| --- | --- |
| Topic | `Mcu/Adc` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the two MCU ADC groups, which sample the voltage of each power rail. |
| Notes | The IDL does not give the conversion from raw ADC code to voltage. |

Interface structure:

```text
struct Adc_ {
    uint16 group1[8];                 // 3V3, 5V, 1V8, 24V-REVA, VIN_60V_CUR, 24V-REVC, 5V-REVB, 5V-REVC
    uint16 group3[8];                 // HardVer, VIN, 5V-REVA, 12V, VDD_5V_A, VDDL_0V9, 60V, VDD_12V
};

```

### Mcu/Temp

| Field | Content |
| --- | --- |
| Topic | `Mcu/Temp` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the onboard local and remote temperatures. |
| Notes | Values are degrees Celsius times 100, that is, 0.01°C/LSB. |

Interface structure:

```text
struct Temp_ {
    int16 local_temp;                 // Local sensor temperature, °C × 100
    int16 remote_temp;                // Remote sensor temperature, °C × 100
};

```

### Mcu/Init

| Field | Content |
| --- | --- |
| Topic | `Mcu/Init` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to MCU power-up information: whether the network is up, IP, uptime, and firmware version. |
| Notes | `version` is at most 32 characters. |

Interface structure:

```text
struct McuInit_ {
    octet network_ok;                 // 0 network down, 1 network up
    octet ip[4];                      // MCU IPv4
    octet netmask[4];                 // Subnet mask
    octet gateway[4];                 // Gateway
    uint16 uptime_sec;                // Uptime, in seconds
    string<32> version;               // Firmware version
    octet reserved;                   // Reserved
};

```

### Mcu/Fault

| Field | Content |
| --- | --- |
| Topic | `Mcu/Fault` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the MCU fault codes that are currently active (not yet cleared). |
| Notes | `node_id` is fixed to 2 when published. The number of valid fault codes is given by `dtc_count`; the list capacity is 32. |

Interface structure:

```text
struct Fault_ {
    octet node_id;                    // Node ID, fixed to 2 when published
    octet lifecycle_state;            // MCU lifecycle state
    octet pwr_state;                  // Power state machine state
    octet dtc_count;                  // Number of current fault codes
    octet reserved;
    uint16 dtc_list[32];              // List of current fault codes
};

```

## Motion Control Commands

`flag[30]` gives a switch per joint. The IDL does not state how the 30 channels map to specific joints.

### rt/enable/motors

| Field | Content |
| --- | --- |
| Topic | `rt/enable/motors` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Enable or disable motors per joint. The HAL subscribes to it. |
| Notes | The IDL does not state the order of the 30 joint channels. |

Interface structure:

```text
struct EnableMotors_ {
    boolean flag[30];                 // Enable per joint
};

```

### rt/clear/errors

| Field | Content |
| --- | --- |
| Topic | `rt/clear/errors` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Request to clear drive errors. The HAL subscribes to it. |
| Notes | `msg_id` identifies this request. |

Interface structure:

```text
struct ClearErrors_ {
    int32 msg_id;                     // Identifier of this request
};

```

### rt/go/home

| Field | Content |
| --- | --- |
| Topic | `rt/go/home` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Go home per joint. The HAL subscribes to it. |
| Notes | The IDL does not state the order of the 30 joint channels. |

Interface structure:

```text
struct GoHome_ {
    boolean flag[30];                 // Go home per joint
};

```

### rt/set/calibration/flag

| Field | Content |
| --- | --- |
| Topic | `rt/set/calibration/flag` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Set the calibration flag per joint. The HAL subscribes to it. |
| Notes | The IDL does not state the order of the 30 joint channels. |

Interface structure:

```text
struct SetCalibrationFlag_ {
    boolean flag[30];                 // Set the calibration flag per joint
};

```

### rt/set/fsm/id

| Field | Content |
| --- | --- |
| Topic | `rt/set/fsm/id` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | The motion control algorithm publishes the current state machine ID and the name of the skill being played. The caller subscribes to it to tell when a skill starts and ends. |
| Notes | `current_action` empty or `"\0"` means playback is not available, `"enable"` means playback is available but nothing is playing, and any other non-empty string is the name of the skill being played. To change the state machine, use the RPC `SetFsmId`; do not publish this topic. The Mini stand mode ID is 300. The example `action_watch` subscribes to this topic. |

Interface structure:

```text
struct SetFsmId_ {
    uint16 id;                        // Motion control state machine ID
    string current_action;            // Empty or "\0": not available; "enable": available but not playing; otherwise the name of the skill being played
};

```

### rt/remote/control

| Field | Content |
| --- | --- |
| Topic | `rt/remote/control` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Publish the remote-control joysticks. The left joystick controls translation, and the right joystick controls turning. |
| Notes | The IDL does not state the joystick value range or the positive axis directions. If velocity commands also go through the RPC `SetVel`, the priority is gamepad over App, and App over RPC. |

Interface structure:

```text
struct RemoteControl_ {
    JoystickValue_ btn_move;         // Left joystick, translation
    JoystickValue_ btn_turn;         // Right joystick, turning
};

struct JoystickValue_ {
    float x;
    float y;
};

```

## Status Detection

### rt/diagnostic/report

| Field | Content |
| --- | --- |
| Topic | `rt/diagnostic/report` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to an aggregated diagnostic report covering motors, IMU, gamepad, performance, locomotion alarms, head, LED, Bluetooth/Wi-Fi, UART, and main nodes. |
| Notes | The HAL subscribes to it and uses it to decide whether startup can continue. Each section uses `data_valid` to indicate whether its data is valid. For the `FaultInfo_` fields, see `dobot_perf_fault`. For the `LocomotionAlarmState_` fields, see `rt/locomotion/alarm/state`. |

Interface structure:

```text
struct DiagnosticReport_ {
    uint64 report_timestamp_ms;
    DiagFaultFrame_ motors;
    DiagFaultFrame_ imu;
    DiagFaultFrame_ gamepad;
    PerformanceAlarmDiagData_ performance_alarm;
    LocomotionAlarmDiagData_ locomotion_alarm;
    HeadDiagData_ head;
    LedDiagData_ led;
    BluetoothWifiDiagData_ bluetooth_wifi;
    UartDiagData_ uart;
    MainNodesStateDiag_ main_nodes_state;
};

struct DiagFaultFrame_ {              // Reused for motors, IMU, and gamepad
    uint16 node_id;
    uint64 timestamp_us;
    uint16 fault_count;
    sequence<uint16> fault_codes;
    boolean data_valid;
};

struct PerformanceAlarmDiagData_ {
    uint64 last_timestamp_us;
    uint64 received_count;
    sequence<FaultInfo_, 64> active_faults;
    boolean data_valid;
};

struct LocomotionAlarmDiagData_ {
    LocomotionAlarmState_ state;
    uint64 received_count;
    boolean data_valid;
};

struct HeadDiagData_ {
    uint16 node_id;
    uint8 eye_effect;
    string firmware_version;
    string touch_version;
    uint32 uptime_s;
    uint32 voltage_mv1;
    uint32 voltage_mv2;
    uint32 error_code;
    string error_message;
    uint64 timestamp_ms;
    boolean data_valid;
};

struct LedDiagData_ {
    uint16 node_id;
    uint32 error_code;
    string message;
    uint64 timestamp_ms;
    boolean data_valid;
};

struct BluetoothWifiDiagData_ {
    uint64 timestamp_ns;
    uint64 seq;
    string node_name;
    string version;
    uint8 node_id;
    uint8 event_type;
    uint8 work_mode;
    uint8 slot;
    uint8 error_code;
    uint8 fault_code;
    string ip;
    string ssid;
    string detail;
    boolean ap_active;
    boolean provision_busy;
    boolean data_valid;
};

struct UartDiagData_ {
    uint8 node_id;
    uint8 lifecycle_state;
    uint8 pwr_state;
    uint8 dtc_count;
    sequence<uint16> dtc_list;
    uint16 heartbeat_seq;
    uint64 timestamp_ms;
    boolean data_valid;
};

struct MainNodesStateDiag_ {
    sequence<AxisStateDiag_> left_leg;
    sequence<AxisStateDiag_> right_leg;
    sequence<AxisStateDiag_> waist;
    sequence<AxisStateDiag_> left_arm;
    sequence<AxisStateDiag_> right_arm;
    sequence<AxisStateDiag_> head;
    sequence<EcatSlaveDiag_> ecat2can;
    uint64 timestamp_ms;
    boolean data_valid;
};

struct AxisStateDiag_ {
    uint8 servo_state;
    uint16 error_code;
    uint16 warn_code;
    int32 pos_err_code;
    int32 vel_err_code;
    int32 torque_err_code;
    uint8 node_state;
    uint8 display_op_mode;
    boolean is_virtual;
    uint8 mcu_temp;
    uint8 mos_temp;
    uint8 motor_temp;
    uint8 bus_voltage;
    uint16 software_version;
};

struct EcatSlaveDiag_ {
    boolean is_virtual;
    uint8 slave_state;
    uint16 error_code;
    uint16 software_version;
};

```

### rt/locomotion/alarm/state

| Field | Content |
| --- | --- |
| Topic | `rt/locomotion/alarm/state` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the alarms of the motion control algorithm that are currently active (not yet cleared). |
| Notes | `active_alarms` holds at most 256 entries. `truncated` true means the list was truncated. `message` is at most 256 characters. |

Interface structure:

```text
const uint8 LOCOMOTION_ALARM_SEVERITY_WARNING = 1;
const uint8 LOCOMOTION_ALARM_SEVERITY_ERROR = 2;
const uint8 LOCOMOTION_ALARM_SEVERITY_CRITICAL = 3;

const uint8 LOCOMOTION_ALARM_SOURCE_UNKNOWN = 0;
const uint8 LOCOMOTION_ALARM_SOURCE_AXIS = 1;
const uint8 LOCOMOTION_ALARM_SOURCE_EMERGENCY = 2;
const uint8 LOCOMOTION_ALARM_SOURCE_IMU = 3;
const uint8 LOCOMOTION_ALARM_SOURCE_GET_UP = 4;
const uint8 LOCOMOTION_ALARM_SOURCE_LIFT = 5;
const uint8 LOCOMOTION_ALARM_SOURCE_LOWER_COMMAND = 6;
const uint8 LOCOMOTION_ALARM_SOURCE_UPPER_COMMAND = 7;
const uint8 LOCOMOTION_ALARM_SOURCE_HAND_COMMAND = 8;
const uint8 LOCOMOTION_ALARM_SOURCE_STARTUP = 9;

struct LocomotionAlarmState_ {
    uint64 seq;
    uint64 timestamp_ms;
    boolean truncated;                // Whether the active alarms were truncated
    sequence<LocomotionAlarmItem_, 256> active_alarms;
};

struct LocomotionAlarmItem_ {
    uint32 code;
    uint8 severity;                   // See the SEVERITY constants
    uint8 source_type;                // See the SOURCE constants
    int32 source_id;
    int32 detail_code;
    double actual_value;
    double expected_value;
    double threshold;
    uint64 first_occurred_ms;
    string<256> message;
};

```

### rt/wifi/status

| Field | Content |
| --- | --- |
| Topic | `rt/wifi/status` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to the heartbeat, work mode, IP, and faults of the Wi-Fi provisioning module. |
| Notes | `seq` increases monotonically and is used for deduplication and ordering. |

Interface structure:

```text
struct WifiStatus_ {
    uint64 timestamp_ns;             // Monotonic clock, in nanoseconds
    uint32 seq;                      // Monotonically increasing sequence number
    string node_name;                // "wifi-provisioning"
    string version;                  // For example V2.0.0-g<hash>
    uint8 node_id;                   // Module ID, default 0x07
    uint8 event_type;                // 0 heartbeat, 1 provisioning started, 2 success, 3 failure, 4 IP changed, 5 fault, 6 exit
    uint8 work_mode;                 // 0 idle, 1 station, 2 ap
    uint8 slot;                      // 0 or 1
    uint8 error_code;                // External error code, 0 is success
    uint8 fault_code;                // Internal fault code, 0x00 is no fault
    string ip;                       // Dotted IPv4. In Station mode the connected IP, in AP mode the hotspot IP
    string ssid;
    string detail;                   // Failure reason or additional information
    boolean ap_active;               // Whether AP hotspot credentials are currently being issued
    boolean provision_busy;          // Whether a provisioning request is being handled
};

```

### dobot\_perf\_fault

| Field | Content |
| --- | --- |
| Topic | `dobot_perf_fault` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to fault or recovery events from the performance monitoring module. |
| Notes | This topic name has no `rt/` prefix. The current type is the performance module's temporary `FaultInfo_`, whose field layout matches the diagnostic module's fault info. `is_recovered` true means this is a recovery event. |

Interface structure:

```text
struct FaultInfo_ {
    uint16 node_id;                  // Node ID, default 0
    uint32 fault_code;               // Fault code within the module
    uint8 fault_level;               // 0 S0, 1 S1, 2 S2, 3 S3
    uint64 timestamp_us;             // Time of occurrence, in microseconds
    string fault_desc;               // Fault description
    uint32 occur_count;              // Accumulated occurrence count
    boolean is_recovered;            // true means the fault has recovered
    string extra_info;               // Extended context
};

```

## Voice

The four messages below all carry `VoiceHeader_`. For failure handling, use `OutputReason`; `detail` is for logging only.

### rt/voice/command

| Field | Content |
| --- | --- |
| Topic | `rt/voice/command` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | Subscribe to speech recognition results. `is_final` distinguishes intermediate results from final results. |
| Notes | `session_id` and `command_id` are UUIDs. Use `auto` when the language cannot be determined. |

Interface structure:

```text
struct VoiceHeader_ {
    string source;                    // Source module name, for example "AgentCore", "SysMonitor"
    int64 timestamp;                  // Unix timestamp, in milliseconds
};

struct VoiceCommand_ {
    VoiceHeader_ header;
    string text;                      // Recognized text
    boolean is_final;                 // true final result, false intermediate result
    double confidence;                // Confidence, 0.0–1.0
    string session_id;                // Dialogue session ID
    string command_id;                // Unique ID generated by the recognizer
    string language;                  // For example zh, en, fr; auto when it cannot be determined
};

```

### rt/voice/tts\_request

| Field | Content |
| --- | --- |
| Topic | `rt/voice/tts_request` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Request to synthesize a piece of text and speak it. |
| Notes | Long text is split into chunks by `seq_num`, starting from 0. `is_final` marks the end of the text. When replying to a user command, fill in the corresponding `VoiceCommand_.command_id`; it can be empty for proactive announcements. |

Interface structure:

```text
enum PriorityLevel {
    PRIORITY_NORMAL,                  // Normal interaction
    PRIORITY_IMPORTANT,               // Important interaction
    PRIORITY_SYSTEM,                  // System warning
    PRIORITY_EMERGENCY                // Emergency / e-stop
};

struct TTSRequest_ {
    VoiceHeader_ header;
    string speak_id;                  // UUID of this speech output
    string text;                      // Text to synthesize
    uint32 seq_num;                   // Chunk sequence number, starting from 0
    PriorityLevel priority;
    boolean interrupt;                // Whether to forcibly interrupt the current speech
    boolean is_final;                 // Whether the text has ended
    string user_data;                 // Reserved, JSON
    string language;                  // Use auto when it cannot be determined
    string reply_to_command_id;       // Fill in command_id when replying to a user command
};

```

### rt/voice/ack\_request

| Field | Content |
| --- | --- |
| Topic | `rt/voice/ack_request` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Request to play an existing fixed audio clip, without passing text to synthesize. |
| Notes | `ack_id` is the ID of a fixed audio asset. Do not pass a file path or text to speak. For `PriorityLevel`, see `rt/voice/tts_request`. |

Interface structure:

```text
struct AckRequest_ {
    VoiceHeader_ header;
    string speak_id;                  // UUID of this fixed-audio output
    string ack_id;                    // Fixed audio asset ID
    PriorityLevel priority;
    boolean interrupt;
    string reply_to_command_id;       // Fill in command_id when it corresponds to a user command; otherwise empty
};

```

### rt/voice/output\_event

| Field | Content |
| --- | --- |
| Topic | `rt/voice/output_event` |
| QoS | No dedicated entry in the library. Uses the Fast DDS default: RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | The speech output module reports whether a request was accepted, and whether playback started, finished, was interrupted, or failed. |
| Notes | Branch on `reason` in your program, not on `detail`. For TTS text chunk events, `has_seq_num` is true; for ACK, control commands, and whole-playback lifecycle events it is false, and `seq_num` should then be ignored. |

Interface structure:

```text
enum OutputRequestKind {
    TTS,
    ACK,
    CONTROL
};

enum OutputEventType {
    REQUEST_BUFFERED,
    REQUEST_ACCEPTED,
    REQUEST_REJECTED,
    REQUEST_DROPPED,
    PLAYBACK_STARTED,
    PLAYBACK_COMPLETED,
    PLAYBACK_INTERRUPTED,
    PLAYBACK_FAILED
};

enum OutputReason {
    NONE,
    DUPLICATE,
    STALE_STREAM,
    STALE_COMMAND,
    INVALID_REQUEST,
    QUEUE_FULL,
    MISSING_SEQUENCE,
    CANCELLED,
    PREEMPTED,
    USER_BARGE_IN,
    UNKNOWN_ACK,
    UNSUPPORTED_CONTROL,
    TTS_FAILED,
    PLAYBACK_FAILED
};

struct VoiceOutputEvent_ {
    VoiceHeader_ header;
    string request_source;            // header.source of the original request
    string speak_id;                  // speak_id of the TTS / ACK; for a control command, the target ID, or empty if there is no target
    string reply_to_command_id;       // reply_to_command_id passed through from the request
    OutputRequestKind request_kind;
    PriorityLevel priority;
    OutputEventType event_type;
    OutputReason reason;
    boolean has_seq_num;              // true for TTS text chunks; false for ACK, control, and whole-playback events
    uint32 seq_num;                   // Ignored when has_seq_num is false
    uint32 expected_seq_num;          // Sequence number of the text chunk currently awaited, on out-of-order, duplicate, or missing chunks
    string detail;                    // For logging only
};

```

## Head

Eye effect values:

| Value | Name |
| --- | --- |
| `0x01` | ANGRY |
| `0x02` | BASE |
| `0x03` | CAREFUL |
| `0x04` | DAZE |
| `0x05` | DEYI |
| `0x06` | GANGA |
| `0x07` | HUAIXIAO |
| `0x08` | JIANDING |
| `0x09` | MOON |
| `0x0A` | SERIOUS |
| `0x0B` | TIRED |
| `0x0C` | WAKEUP |
| `0x0D` | WINK1 |
| `0x0E` | YUNTOU |
| `0x0F` | ZHENWANG |
| `0x10` | WINK2 |
| `0x11` | GLANCE2 |

The `color` field of `rt/head/eye_effect_cmd` takes effect only when `eye_effect` is `0xFF`.

### rt/head/eye\_effect\_cmd

| Field | Content |
| --- | --- |
| Topic | `rt/head/eye_effect_cmd` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Set the head eye effect, along with its hold time and color. |
| Notes | `control_node_id` is recorded only; the head does not use it for routing. `duration_ms` of 0 means unspecified, and the MCU uses its default hold time. |

Interface structure:

```text
struct HeadEyeEffectCmd_ {
    uint16 control_node_id;           // Recorded only
    uint8 eye_effect;                 // HEAD_EYE_EFFECT_*; color is enabled at 0xFF
    uint16 duration_ms;               // Hold time after playback completes, in milliseconds; 0 is the MCU default
    uint32 color;                     // RGB888, lower 24 bits valid
    uint64 timestamp_ms;              // Unix milliseconds
};

```

### rt/head/info\_request

| Field | Content |
| --- | --- |
| Topic | `rt/head/info_request` |
| QoS | RELIABLE, KEEP\_LAST, VOLATILE, depth 1 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | publish |
| Description | Request device information from the head once. |
| Notes | The head replies on `rt/head/state` only after it receives the request. It does not push periodically. |

Interface structure:

```text
struct HeadInfoRequest_ {
    uint16 control_node_id;           // Recorded only
    uint64 timestamp_ms;              // Request timestamp
};

```

### rt/head/state

| Field | Content |
| --- | --- |
| Topic | `rt/head/state` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | The head replies with its current eye effect, firmware version, touch version, uptime, and voltage. |
| Notes | Sent only after `rt/head/info_request` is received. It is not a periodic state. |

Interface structure:

```text
struct HeadState_ {
    uint16 control_node_id;
    uint8 eye_effect;                 // Current eye effect
    string firmware_version;          // For example "V1.2.3"
    string touch_version;             // For example "V1.2.3"
    uint32 uptime_s;                  // Uptime, in seconds
    uint32 voltage_mv1;               // Voltage 1, in mV
    uint32 voltage_mv2;               // Voltage 2, in mV
    uint64 timestamp_ms;              // Publish time, Unix milliseconds
};

```

### rt/head/touch\_event

| Field | Content |
| --- | --- |
| Topic | `rt/head/touch_event` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | The head reports which touch areas are currently pressed. |
| Notes | `pressed` can contain multiple areas at once. The mapping between enum values 0–4 and physical keys is in the head bridge; the IDL only gives the area names. |

Interface structure:

```text
enum HeadTouchButton {
    HEAD_TOUCH_TOP,
    HEAD_TOUCH_LEFT_0,
    HEAD_TOUCH_LEFT_1,
    HEAD_TOUCH_RIGHT_0,
    HEAD_TOUCH_RIGHT_1
};

struct HeadTouchEvent_ {
    uint16 control_node_id;
    sequence<HeadTouchButton> pressed; // Areas currently pressed, multiple allowed
    uint64 timestamp_ms;
};

```

### rt/head/error\_event

| Field | Content |
| --- | --- |
| Topic | `rt/head/error_event` |
| QoS | BEST\_EFFORT, KEEP\_LAST, VOLATILE, depth 2 |
| Effective version | Current SDK; shared by SR\_3588 and SR\_thor |
| Access | subscribe |
| Description | The head reports an error code and a text description. |
| Notes | `error_code` can be a head SDK error code or an MCU error code. |

Interface structure:

```text
struct HeadErrorEvent_ {
    uint16 control_node_id;
    uint32 error_code;
    string message;
    uint64 timestamp_ms;
};

```
