# DDS 话题说明

范围是当前 SDK 已经挂到话题常量上的 DDS 话题。SR\_3588 与 SR\_thro 的话题名相同，常量分别在 `dobot::robot::sr_3588` 和 `dobot::robot::sr_thro`。消息类型在 `dobot::msg::dds_`。

调用方通过 `Session::Subscribe` 订阅、`Session::Publish` 发布。方向按调用方写：订阅表示机器人在发，发布表示调用方在发。默认 domain 为 0。QoS 编译在库里，按「话题名.reader」和「话题名.writer」选取，历史都是 KEEP\_LAST、VOLATILE。下肢和上肢的状态与命令、主节点状态，以及遥控，是 BEST\_EFFORT、深度 2。骨盆 IMU、躯干 IMU 和手柄是 BEST\_EFFORT、深度 4。使能、清错、回零、标零、`rt/set/fsm/id`、急停、电池、MCU 上电、诊断，以及头部表情和信息请求，是 RELIABLE、深度 1。头部状态、触摸和错误是 BEST\_EFFORT、深度 2。库里没有条目的话题使用 Fast DDS 默认 QoS（RELIABLE、KEEP\_LAST、深度 1），对端若是 BEST\_EFFORT 会匹配不上。逐话题 QoS 见 [dobot-SDK使用指南.md](dobot-SDK%E4%BD%BF%E7%94%A8%E6%8C%87%E5%8D%97.md) 第 4 节。

仓库没有按话题记录生效版本。下表「生效版本」统一写当前源码已经提供，两台型号共用。

示例程序覆盖了下肢 / 上肢状态、下肢命令和 MCU 急停。`action_watch` 订阅 `rt/set/fsm/id`，用来看技能开始和结束。其余话题的名字和类型已经在 SDK 里，调用方式相同。

关节角、角速度、力矩、电流、温度和 IMU 原始量的单位，IDL 大多没有写死。二次开发说明里这些单位仍待确认，下文只保留 IDL 里已经写明的单位。

| Topic | 接入方式 | 类型 | 说明 |
| --- | --- | --- | --- |
| `rt/lower/state` | 订阅 | `LowerState_` | 下肢关节反馈 |
| `rt/upper/state` | 订阅 | `UpperState_` | 上肢关节反馈 |
| `rt/lower/cmd` | 发布 | `LowerCmd_` | 下肢关节命令，12 路 |
| `rt/upper/cmd` | 发布 | `UpperCmd_` | 上肢关节命令，18 路 |
| `rt/stand/cmd` | 发布 | `StandingControl_` | Mini 站立模式下的头、腰和身高 |
| `rt/pelvis/imu/data` | 订阅 | `IMUState_` | 骨盆 IMU |
| `rt/torso/imu/data` | 订阅 | `IMUState_` | 躯干 IMU |
| `rt/gamepad/state` | 订阅 | `GamepadState_` | 手柄按键和摇杆 |
| `rt/main/nodes/state` | 订阅 | `MainNodesState_` | 伺服节点与 EtherCAT 从站 |
| `Mcu/BmsCommon` | 订阅 | `BmsCommon_` | 电池常用遥测 |
| `Mcu/BmsUncommon` | 订阅 | `BmsUncommon_` | 电池逐节电压与版本 |
| `Mcu/Heartbeat` | 订阅 | `Heartbeat_` | MCU 心跳 |
| `Mcu/Estop` | 订阅 | `Estop_` | 两路急停引脚 |
| `Mcu/Adc` | 订阅 | `Adc_` | MCU 电源 ADC |
| `Mcu/Temp` | 订阅 | `Temp_` | 板载温度 |
| `Mcu/Init` | 订阅 | `McuInit_` | MCU 上电信息 |
| `Mcu/Fault` | 订阅 | `Fault_` | MCU 未消除故障码 |
| `rt/enable/motors` | 发布 | `EnableMotors_` | 按关节开关使能 |
| `rt/clear/errors` | 发布 | `ClearErrors_` | 请求清除驱动错误 |
| `rt/go/home` | 发布 | `GoHome_` | 按关节回零 |
| `rt/set/calibration/flag` | 发布 | `SetCalibrationFlag_` | 按关节设置标零标志 |
| `rt/set/fsm/id` | 订阅 | `SetFsmId_` | 运控算法发布的状态机编号和当前技能名 |
| `rt/remote/control` | 发布 | `RemoteControl_` | 遥控摇杆 |
| `rt/diagnostic/report` | 订阅 | `DiagnosticReport_` | 汇总诊断 |
| `rt/locomotion/alarm/state` | 订阅 | `LocomotionAlarmState_` | 运控未消除告警 |
| `rt/wifi/status` | 订阅 | `WifiStatus_` | Wi-Fi 配网状态 |
| `dobot_perf_fault` | 订阅 | `FaultInfo_` | 性能监测故障或恢复 |
| `rt/voice/command` | 订阅 | `VoiceCommand_` | 语音识别结果 |
| `rt/voice/tts_request` | 发布 | `TTSRequest_` | 请求合成并播报文本 |
| `rt/voice/ack_request` | 发布 | `AckRequest_` | 请求播放固定音频 |
| `rt/voice/output_event` | 订阅 | `VoiceOutputEvent_` | 播报受理与播放事件 |
| `rt/head/eye_effect_cmd` | 发布 | `HeadEyeEffectCmd_` | 设置眼部表情 |
| `rt/head/info_request` | 发布 | `HeadInfoRequest_` | 请求头部设备信息 |
| `rt/head/state` | 订阅 | `HeadState_` | 头部设备信息回复 |
| `rt/head/touch_event` | 订阅 | `HeadTouchEvent_` | 头部触摸 |
| `rt/head/error_event` | 订阅 | `HeadErrorEvent_` | 头部错误 |

## 关节状态与命令

### rt/lower/state

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/lower/state` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅下肢关节反馈。机载 HAL 周期发布位置、速度、力矩和温度。 |
| 备注 | 状态数组长度 15。二次开发说明里的下标约定是：前 12 路与 `rt/lower/cmd` 对齐，0–5 为左腿，6–11 为右腿。多出的 3 路含义、每一路关节名和正方向尚未写明。`q`、`dq`、力矩和温度的单位 IDL 未标注。 |

接口结构：

```text
struct LowerState_ {
    MotorState_ motor_state[15];
    uint64 timestamp_us;              // 时间戳，单位微秒
};

struct MotorState_ {
    uint8 mode;                       // 模式
    float q;                          // 角位置
    float dq;                         // 角速度
    float ddq;                        // 角加速度
    float tau_est;                    // 估计的力矩
    float tau_cmd;                    // 指令力矩
    float current_feedback;           // 反馈电流
    float current_cmd;                // 指令电流
    float q_raw;                      // 原始角位置
    float dq_raw;                     // 原始角速度
    float ddq_raw;                    // 原始角加速度
    float tau_raw;                    // 原始力矩
    uint8 mcu_temp;                   // 伺服控制板温度
    uint8 mos_temp;                   // MOS 管温度
    uint8 motor_temp;                 // 电机温度
    uint8 bus_voltage;                // 母线电压
};

```

### rt/upper/state

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/upper/state` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅上肢关节反馈，同时带机型名，以及上肢当前有没有在受控。 |
| 备注 | `robot_type` 是长度 16 的字符数组，取值表尚未确认。`is_upper_control` 为 true 表示上肢有控制，false 表示上肢无控制。`motor_state` 长度 18。二次开发说明里的下标约定是：0–2 腰，3–9 左臂，10–16 右臂，17 头。每一路关节名、正方向，以及头的路数是否与该约定一致，尚未确认。`MotorState_` 字段同 `rt/lower/state`。 |

接口结构：

```text
struct UpperState_ {
    char robot_type[16];              // 产品定义的机型名
    boolean is_upper_control;         // true：上肢有控制；false：上肢无控制
    MotorState_ motor_state[18];
    uint64 timestamp_us;              // 时间戳，单位微秒
};

```

### rt/lower/cmd

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/lower/cmd` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 发布下肢关节命令。HAL 订阅后下发给电机，共 12 路。 |
| 备注 | 下标约定：0–5 左腿，6–11 右腿。每一路关节名和正方向尚未写明。`mode` 的允许取值，以及全 0 帧会不会被当成真实命令，尚未确认。示例 `dds_publish_cmd` 只有加上 `--send` 才会发一帧全 0 命令，用来确认写端匹配。发关节命令前需要确认状态机处于允许底层控制的模式，并且写端已经和机器人匹配。 |

接口结构：

```text
struct LowerCmd_ {
    MotorCmd_ motor_cmd[12];          // 12 路电机命令
};

struct MotorCmd_ {
    uint8 mode;                       // 模式
    float q;                          // 位置
    float dq;                         // 速度
    float tau;                        // 力矩
    float kp;                         // 比例增益
    float kd;                         // 微分增益
};

```

### rt/upper/cmd

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/upper/cmd` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 发布上肢关节命令。HAL 订阅后下发给电机。 |
| 备注 | 数组长度是 18。IDL 注释写成「17 个」，以数组长度 18 为准。下标约定同 `rt/upper/state`：0–2 腰，3–9 左臂，10–16 右臂，17 头。`MotorCmd_` 字段同 `rt/lower/cmd`。 |

接口结构：

```text
struct UpperCmd_ {
    MotorCmd_ motor_cmd[18];          // 18 路电机命令
};

```

### rt/stand/cmd

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/stand/cmd` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 在 Mini 站立模式（状态机 300）下，同时设置头偏航、腰偏航和身体高度。 |
| 备注 | 只在状态机 300 下接受。每一帧同时替换这三个目标。腰的俯仰和横滚不在这里控制。发送前先用 RPC `SetFsmId(300)` 进入站立模式。 |

接口结构：

```text
struct StandingControl_ {
    float head_yaw;                   // 头部偏航绝对目标，单位 rad
    float waist_yaw;                  // 相对默认姿态的腰偏航偏移，单位 rad
    float body_height;                // 身体高度目标，单位 m；0 恢复默认站立高度
};

```

## IMU

两路都是 `IMUState_`。用话题和 `imu_id` 区分安装位置。四元数顺序、陀螺仪、加速度和 `rpy` 的单位，IDL 未标注。

### rt/pelvis/imu/data

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/pelvis/imu/data` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 4 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅骨盆 IMU。 |
| 备注 | 发布时 `imu_id` 为 1。当前 HAL 主循环周期发布这一路。 |

接口结构：

```text
struct IMUState_ {
    uint32 imu_id;                    // IMU 编号。骨盆为 1
    float quaternion[4];              // 四元数
    float gyroscope[3];               // 陀螺仪
    float accelerometer[3];           // 加速度计
    float rpy[3];                     // 欧拉角：滚转、俯仰、偏航
    uint8 temperature;                // 温度
    uint64 timestamp_us;              // 时间戳，单位微秒
};

```

### rt/torso/imu/data

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/torso/imu/data` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 4 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅躯干 IMU。字段与骨盆 IMU 相同。 |
| 备注 | 发布时 `imu_id` 为 2。发布函数已有，当前 HAL 主循环没有调用这一路。 |

接口结构：

```text
struct IMUState_ {
    uint32 imu_id;                    // IMU 编号。躯干为 2
    float quaternion[4];
    float gyroscope[3];
    float accelerometer[3];
    float rpy[3];                     // 滚转、俯仰、偏航
    uint8 temperature;
    uint64 timestamp_us;              // 时间戳，单位微秒
};

```

## 手柄与主节点

### rt/gamepad/state

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/gamepad/state` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 4 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅手柄按键和左右摇杆。HAL 读到手柄后发布。 |
| 备注 | 摇杆数值范围 IDL 未标注。 |

接口结构：

```text
struct GamepadState_ {
    uint16 head;                      // 协议头
    uint16 btn_state;
    // bit0: R1     bit8:  A
    // bit1: L1     bit9:  B
    // bit2: start  bit10: X
    // bit3: select bit11: Y
    // bit4: R2     bit12: up
    // bit5: L2     bit13: right
    // bit6: F1     bit14: down
    // bit7: F2     bit15: left
    float lx;                         // 左摇杆 x
    float ly;                         // 左摇杆 y
    float rx;                         // 右摇杆 x
    float ry;                         // 右摇杆 y
    uint64 timestamp_us;              // 时间戳，单位微秒
};

```

### rt/main/nodes/state

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/main/nodes/state` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅腿、腰、臂、头各伺服节点，以及 EtherCAT 从站的状态、报警、温度、版本和标零结果。 |
| 备注 | 手和电源状态在 IDL 里被注释掉，当前消息不包含。温度和母线电压的单位 IDL 未标注。 |

接口结构：

```text
struct MainNodesState_ {
    AxisStateInfo_ left_leg[6];
    AxisStateInfo_ right_leg[6];
    AxisStateInfo_ waist[3];
    AxisStateInfo_ left_arm[7];
    AxisStateInfo_ right_arm[7];
    AxisStateInfo_ head[2];
    EcatSlaveInfo_ ecat2can[2];
    uint64 timestamp_us;              // 时间戳，单位微秒
};

struct AxisStateInfo_ {
    uint8 servo_state;
    uint16 error_code;                // 电机报警错误码
    uint16 warn_code;                 // 电机警告错误码
    int32 pos_err_code;
    int32 vel_err_code;
    int32 torque_err_code;
    uint8 node_state;
    uint8 display_op_mode;
    boolean is_virtual;               // 默认 false
    uint8 mcu_temp;
    uint8 mos_temp;
    uint8 motor_temp;
    uint8 bus_voltage;
    uint16 software_version;
    uint8 zero_calib;                 // 标零状态：0 未成功，1 成功，2 未知
};

struct EcatSlaveInfo_ {
    boolean is_virtual;
    uint8 slave_state;
    uint16 error_code;
    uint16 software_version;
};

```

## 电池与 MCU

电量百分比在 `BmsCommon_.remaining_pct`。这些话题由 MCU 发布，调用方订阅。

### Mcu/BmsCommon

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/BmsCommon` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅电池常用遥测：充放电、电压、电流、温度和剩余电量。 |
| 备注 | 充电电流为负。 |

接口结构：

```text
struct BmsCommon_ {
    octet protection[8];              // Protection1/2、Fault1、Underlying1/2、Reserved、NTC、Reserved
    boolean charging;                 // 充电状态
    boolean discharging;              // 放电状态
    uint16 max_cell_mv;               // 最高单节电芯电压，单位 mV
    uint16 min_cell_mv;               // 最低单节电芯电压，单位 mV
    uint16 total_battery_10mv;        // 电池总电压，10 mV/格
    uint16 battery_output_10mv;       // 电池输出端电压，10 mV/格
    uint16 battery_cplus_10mv;        // 电池 C+ 电压，10 mV/格
    uint16 max_cell_temp_x10;         // 最高电芯温度，0.1°C/格
    uint16 min_cell_temp_x10;         // 最低电芯温度，0.1°C/格
    uint16 chg_mos_temp_x10;          // 充电 MOS 温度，0.1°C/格
    uint16 dchg_mos_temp_x10;         // 放电 MOS 温度，0.1°C/格
    uint16 prechg_mos_temp_x10;       // 预充电 MOS 温度，0.1°C/格
    uint16 max_mos_temp_x10;          // MOS 最高温度，0.1°C/格
    int32 current_ma;                 // 实时电流，单位 mA，充电为负
    octet remaining_pct;              // 剩余容量百分比，0–100
    uint16 remaining_mah;             // 剩余容量，单位 mAh
    uint16 full_charge_mah;           // 满充容量，单位 mAh
    uint16 cycle_count;               // 充放电循环次数
    octet soh_pct;                    // 电池健康度 SOH，0–100
};

```

### Mcu/BmsUncommon

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/BmsUncommon` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅电池逐节电压、电芯温度、累计电量和软硬件版本。 |
| 备注 | 无。 |

接口结构：

```text
struct BmsUncommon_ {
    uint16 cell_mv[20];               // 20 节电芯电压，单位 mV
    int16 cell_temp_x10[8];           // 8 路电芯温度，0.1°C/格
    octet calib_rsvd[8];              // 电流校准参数 / 保留
    uint32 charge_energy_pct;         // 累计充电电量，1% 单位
    uint32 discharge_energy_pct;      // 累计放电电量，1% 单位
    uint16 hw_version;                // BMS 硬件版本
    uint16 sw_version;                // BMS 软件版本
};

```

### Mcu/Heartbeat

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/Heartbeat` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅 MCU 心跳。序号递增。 |
| 备注 | 无。 |

接口结构：

```text
struct Heartbeat_ {
    uint16 seq;                       // 心跳序号，递增
};

```

### Mcu/Estop

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/Estop` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅两路急停引脚。 |
| 备注 | 0 是急停，1 是正常。示例 `dds_event` 订阅这一路，收到一帧就进入回调。 |

接口结构：

```text
struct Estop_ {
    octet stop1;                      // MCU_STOP1：0 急停，1 正常
    octet stop2;                      // MCU_STOP2：0 急停，1 正常
};

```

### Mcu/Adc

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/Adc` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅 MCU 两组 ADC，采各路电源电压。 |
| 备注 | 原始 ADC 码到电压的换算 IDL 未写。 |

接口结构：

```text
struct Adc_ {
    uint16 group1[8];                 // 3V3、5V、1V8、24V-REVA、VIN_60V_CUR、24V-REVC、5V-REVB、5V-REVC
    uint16 group3[8];                 // HardVer、VIN、5V-REVA、12V、VDD_5V_A、VDDL_0V9、60V、VDD_12V
};

```

### Mcu/Temp

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/Temp` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅板载本地温度和远程温度。 |
| 备注 | 数值是摄氏度乘 100，也就是 0.01°C/格。 |

接口结构：

```text
struct Temp_ {
    int16 local_temp;                 // 本地传感器温度，°C × 100
    int16 remote_temp;                // 远程传感器温度，°C × 100
};

```

### Mcu/Init

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/Init` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅 MCU 上电信息：网络是否起来、IP、运行时间和固件版本。 |
| 备注 | `version` 最长 32 字符。 |

接口结构：

```text
struct McuInit_ {
    octet network_ok;                 // 0 网络未起来，1 网络已起来
    octet ip[4];                      // MCU IPv4
    octet netmask[4];                 // 子网掩码
    octet gateway[4];                 // 网关
    uint16 uptime_sec;                // 运行时长，单位秒
    string<32> version;               // 固件版本
    octet reserved;                   // 保留
};

```

### Mcu/Fault

| 字段 | 内容 |
| --- | --- |
| Topic | `Mcu/Fault` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅 MCU 当前还没消除的故障码。 |
| 备注 | 发布时 `node_id` 固定为 2。有效故障码个数看 `dtc_count`，列表容量 32。 |

接口结构：

```text
struct Fault_ {
    octet node_id;                    // 节点 ID，发布时固定为 2
    octet lifecycle_state;            // MCU 生命周期状态
    octet pwr_state;                  // 电源状态机状态
    octet dtc_count;                  // 当前故障码数量
    octet reserved;
    uint16 dtc_list[32];              // 当前故障码列表
};

```

## 运控命令

`flag[30]` 按关节给出开关。30 路与具体关节的对应顺序，IDL 没有写明。

### rt/enable/motors

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/enable/motors` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 按关节开关使能。HAL 订阅。 |
| 备注 | 30 路关节顺序 IDL 未写明。 |

接口结构：

```text
struct EnableMotors_ {
    boolean flag[30];                 // 按关节使能
};

```

### rt/clear/errors

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/clear/errors` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 请求清除驱动错误。HAL 订阅。 |
| 备注 | `msg_id` 用来标识这一次请求。 |

接口结构：

```text
struct ClearErrors_ {
    int32 msg_id;                     // 本次请求标识
};

```

### rt/go/home

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/go/home` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 按关节回零。HAL 订阅。 |
| 备注 | 30 路关节顺序 IDL 未写明。 |

接口结构：

```text
struct GoHome_ {
    boolean flag[30];                 // 按关节回零
};

```

### rt/set/calibration/flag

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/set/calibration/flag` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 按关节设置标零标志。HAL 订阅。 |
| 备注 | 30 路关节顺序 IDL 未写明。 |

接口结构：

```text
struct SetCalibrationFlag_ {
    boolean flag[30];                 // 按关节设置标零标志
};

```

### rt/set/fsm/id

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/set/fsm/id` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 运控算法发布当前状态机编号，以及正在播放的技能名。调用方订阅，用来判断技能开始和结束。 |
| 备注 | `current_action` 为空或 `"\0"` 表示不可播放，`"enable"` 表示可播放但没在播，其它非空字符串是正在播放的技能名。改状态机走 RPC `SetFsmId`，不要发布这个话题。Mini 站立模式编号是 300。示例 `action_watch` 订阅这一路。 |

接口结构：

```text
struct SetFsmId_ {
    uint16 id;                        // 运控状态机编号
    string current_action;            // 空或 "\0" 不可播放；"enable" 可播放但未在播；其它为正在播放的技能名
};

```

### rt/remote/control

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/remote/control` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 发布遥控摇杆。左摇杆管平移，右摇杆管转向。 |
| 备注 | 摇杆数值范围和轴向正方向 IDL 未标注。速度指令若同时走 RPC `SetVel`，优先级是手柄高于 App，App 高于 RPC。 |

接口结构：

```text
struct RemoteControl_ {
    JoystickValue_ btn_move;         // 左摇杆，平移
    JoystickValue_ btn_turn;         // 右摇杆，转向
};

struct JoystickValue_ {
    float x;
    float y;
};

```

## 状态检测

### rt/diagnostic/report

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/diagnostic/report` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅一份汇总诊断，覆盖电机、IMU、手柄、性能、运控告警、头、灯、蓝牙 Wi-Fi、串口和主节点。 |
| 备注 | HAL 订阅后用来判断能不能继续启动。各分块用 `data_valid` 表示这份数据是否有效。`FaultInfo_` 字段见 `dobot_perf_fault`。`LocomotionAlarmState_` 字段见 `rt/locomotion/alarm/state`。 |

接口结构：

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

struct DiagFaultFrame_ {              // 电机、IMU、手柄复用
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

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/locomotion/alarm/state` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅运控算法当前还没消除的告警。 |
| 备注 | `active_alarms` 最多 256 条。`truncated` 为 true 表示列表被截断。`message` 最长 256 字符。 |

接口结构：

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
    boolean truncated;                // 活动告警是否被截断
    sequence<LocomotionAlarmItem_, 256> active_alarms;
};

struct LocomotionAlarmItem_ {
    uint32 code;
    uint8 severity;                   // 见 SEVERITY 常量
    uint8 source_type;                // 见 SOURCE 常量
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

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/wifi/status` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅 Wi-Fi 配网模块的心跳、工作模式、IP 和故障。 |
| 备注 | `seq` 单调递增，用于去重和排序。 |

接口结构：

```text
struct WifiStatus_ {
    uint64 timestamp_ns;             // 单调时钟，单位纳秒
    uint32 seq;                      // 单调递增序号
    string node_name;                // "wifi-provisioning"
    string version;                  // 例如 V2.0.0-g<hash>
    uint8 node_id;                   // 模块标识，默认 0x07
    uint8 event_type;                // 0 心跳，1 配网开始，2 成功，3 失败，4 IP 变化，5 故障，6 退出
    uint8 work_mode;                 // 0 idle，1 station，2 ap
    uint8 slot;                      // 0 或 1
    uint8 error_code;                // 对外错误码，0 为成功
    uint8 fault_code;                // 内部故障码，0x00 为无故障
    string ip;                       // 点分 IPv4。Station 为所连 IP，AP 为热点 IP
    string ssid;
    string detail;                   // 失败原因或补充信息
    boolean ap_active;               // AP 热点凭据是否正在下发
    boolean provision_busy;          // 是否正在处理配网请求
};

```

### dobot\_perf\_fault

| 字段 | 内容 |
| --- | --- |
| Topic | `dobot_perf_fault` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅性能监测模块发出的故障或恢复事件。 |
| 备注 | 这条话题名没有 `rt/` 前缀。当前类型是性能模块的临时 `FaultInfo_`，字段布局与诊断模块的故障信息一致。`is_recovered` 为 true 表示这是一次恢复事件。 |

接口结构：

```text
struct FaultInfo_ {
    uint16 node_id;                  // 节点 ID，默认 0
    uint32 fault_code;               // 模块内故障码
    uint8 fault_level;               // 0 S0，1 S1，2 S2，3 S3
    uint64 timestamp_us;             // 发生时间，单位微秒
    string fault_desc;               // 故障描述
    uint32 occur_count;              // 累计发生次数
    boolean is_recovered;            // true 表示故障恢复
    string extra_info;               // 扩展上下文
};

```

## 语音

下面四条消息都带 `VoiceHeader_`。失败处理看 `OutputReason`，`detail` 只用于日志。

### rt/voice/command

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/voice/command` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 订阅语音识别结果。`is_final` 区分中间结果和最终结果。 |
| 备注 | `session_id` 和 `command_id` 为 UUID。语言无法确定时填 `auto`。 |

接口结构：

```text
struct VoiceHeader_ {
    string source;                    // 来源模块名，例如 "AgentCore"、"SysMonitor"
    int64 timestamp;                  // Unix 时间戳，单位毫秒
};

struct VoiceCommand_ {
    VoiceHeader_ header;
    string text;                      // 识别文本
    boolean is_final;                 // true 最终结果，false 中间结果
    double confidence;                // 置信度，0.0–1.0
    string session_id;                // 对话会话 ID
    string command_id;                // 识别侧生成的唯一标识
    string language;                  // 如 zh、en、fr；无法确定时为 auto
};

```

### rt/voice/tts\_request

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/voice/tts_request` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 请求把一段文本合成并播报。 |
| 备注 | 长文本按 `seq_num` 分片，从 0 开始。`is_final` 表示文本结束。回复用户指令时填写对应的 `VoiceCommand_.command_id`，主动播报可以为空。 |

接口结构：

```text
enum PriorityLevel {
    PRIORITY_NORMAL,                  // 普通交互
    PRIORITY_IMPORTANT,               // 重要交互
    PRIORITY_SYSTEM,                  // 系统警告
    PRIORITY_EMERGENCY                // 紧急 / 急停
};

struct TTSRequest_ {
    VoiceHeader_ header;
    string speak_id;                  // 本次播报 UUID
    string text;                      // 待合成文本
    uint32 seq_num;                   // 分片序号，从 0 开始
    PriorityLevel priority;
    boolean interrupt;                // 是否强制打断当前播报
    boolean is_final;                 // 文本是否结束
    string user_data;                 // 预留，JSON
    string language;                  // 无法确定时填 auto
    string reply_to_command_id;       // 回复用户指令时填写 command_id
};

```

### rt/voice/ack\_request

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/voice/ack_request` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 请求播放一段已有的固定音频，不传待合成文本。 |
| 备注 | `ack_id` 是固定音频资产 ID，不传文件路径，也不传待播报文本。`PriorityLevel` 见 `rt/voice/tts_request`。 |

接口结构：

```text
struct AckRequest_ {
    VoiceHeader_ header;
    string speak_id;                  // 本次固定音频播报的 UUID
    string ack_id;                    // 固定音频资产 ID
    PriorityLevel priority;
    boolean interrupt;
    string reply_to_command_id;       // 对应用户指令时填写 command_id，否则为空
};

```

### rt/voice/output\_event

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/voice/output_event` |
| QoS | 库内无单独条目。使用 Fast DDS 默认：RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 播报模块回报请求有没有被接受，以及播放开始、结束、打断或失败。 |
| 备注 | 程序分支看 `reason`，不要看 `detail`。TTS 文本块事件 `has_seq_num` 为 true；ACK、控制命令和整段播放生命周期事件为 false，此时忽略 `seq_num`。 |

接口结构：

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
    string request_source;            // 原始请求的 header.source
    string speak_id;                  // TTS / ACK 的 speak_id；控制命令对应目标 ID，没有目标时为空
    string reply_to_command_id;       // 透传请求里的 reply_to_command_id
    OutputRequestKind request_kind;
    PriorityLevel priority;
    OutputEventType event_type;
    OutputReason reason;
    boolean has_seq_num;              // TTS 文本块为 true；ACK、控制和整段播放事件为 false
    uint32 seq_num;                   // has_seq_num 为 false 时忽略
    uint32 expected_seq_num;          // 乱序、重复或缺包时，当前等待的文本块序号
    string detail;                    // 只用于日志
};

```

## 头部

眼部表情取值：

| 值 | 名称 |
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

`eye_effect` 为 `0xFF` 时，`rt/head/eye_effect_cmd` 的 `color` 才启用。

### rt/head/eye\_effect\_cmd

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/head/eye_effect_cmd` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 设置头部眼部表情，以及保持时间和颜色。 |
| 备注 | `control_node_id` 只作记录，头部不据此选路。`duration_ms` 为 0 表示不指定，由 MCU 用默认保持时间。 |

接口结构：

```text
struct HeadEyeEffectCmd_ {
    uint16 control_node_id;           // 仅记录
    uint8 eye_effect;                 // HEAD_EYE_EFFECT_*；0xFF 时启用 color
    uint16 duration_ms;               // 播放完成后的保持时间，单位毫秒；0 为 MCU 默认
    uint32 color;                     // RGB888，低 24 位有效
    uint64 timestamp_ms;              // Unix 毫秒
};

```

### rt/head/info\_request

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/head/info_request` |
| QoS | RELIABLE，KEEP\_LAST，VOLATILE，深度 1 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 发布 \| publish |
| 接口说明 | 向头部要一次设备信息。 |
| 备注 | 头部收到请求后才在 `rt/head/state` 回复，不会周期推送。 |

接口结构：

```text
struct HeadInfoRequest_ {
    uint16 control_node_id;           // 仅记录
    uint64 timestamp_ms;              // 请求时间戳
};

```

### rt/head/state

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/head/state` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 头部回复当前表情、固件版本、触摸版本、运行时间和电压。 |
| 备注 | 收到 `rt/head/info_request` 后才发，不是周期状态。 |

接口结构：

```text
struct HeadState_ {
    uint16 control_node_id;
    uint8 eye_effect;                 // 当前眼部效果
    string firmware_version;          // 例如 "V1.2.3"
    string touch_version;             // 例如 "V1.2.3"
    uint32 uptime_s;                  // 运行时间，单位秒
    uint32 voltage_mv1;               // 电压 1，单位 mV
    uint32 voltage_mv2;               // 电压 2，单位 mV
    uint64 timestamp_ms;              // 发布时间，Unix 毫秒
};

```

### rt/head/touch\_event

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/head/touch_event` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 头部上报哪些触摸区域正被按下。 |
| 备注 | `pressed` 可以同时包含多个区域。枚举值 0–4 与物理键的对应关系在头部桥接里，IDL 只给了区域名。 |

接口结构：

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
    sequence<HeadTouchButton> pressed; // 当前按下的区域，可多键
    uint64 timestamp_ms;
};

```

### rt/head/error\_event

| 字段 | 内容 |
| --- | --- |
| Topic | `rt/head/error_event` |
| QoS | BEST\_EFFORT，KEEP\_LAST，VOLATILE，深度 2 |
| 生效版本 | 当前 SDK；SR\_3588 与 SR\_thro 共用 |
| 接入方式 | 订阅 \| subscribe |
| 接口说明 | 头部上报错误码和文字说明。 |
| 备注 | `error_code` 可能是头部 SDK 错误码，也可能是 MCU 错误码。 |

接口结构：

```text
struct HeadErrorEvent_ {
    uint16 control_node_id;
    uint32 error_code;
    string message;
    uint64 timestamp_ms;
};

```