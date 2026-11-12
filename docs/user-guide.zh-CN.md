[English](./user-guide.md) | **简体中文**

# dobot-sdk 使用指南

这份说明给拿到 `dobot-sdk` 目录的二次开发阅读。目录里是头文件、预编译库和已经编好的示例，调用方用 C++17 链接，不编译 SDK 实现。

SDK 提供两条通道：

*   **RPC** 走 TCP，一次调用做一件事：查状态机、切状态机、设速度、加载技能、播放动作。
    
*   **DDS** 用来持续收状态，或按自己的周期发命令。订阅和发布都从 `dobot::sdk::Session` 进入。
    

两台机器人的 RPC 方法和 DDS 话题相同。差别是命名空间、默认 IP，以及各自的一份话题常量。

| 型号 | 命名空间 | 话题常量头文件 | 默认 IP | RPC 端口 | DDS domain |
| --- | --- | --- | --- | --- | --- |
| SR\_3588 | `dobot::robot::sr_3588` | `include/dobot/robot/sr_3588/topics.hpp` | `192.168.5.2` | `51234` | `0` |
| SR\_thor | `dobot::robot::sr_thor` | `include/dobot/robot/sr_thor/topics.hpp` | `172.16.20.2` | `51234` | `0` |

下文示例用 SR\_3588。换成 SR\_thor 时，把 `dobot::robot::sr_3588` 换成 `dobot::robot::sr_thor`，IP 换成 `172.16.20.2`。话题字符串不用改。

需要 C++17 和 CMake 3.16 或更高版本。预编译库要求 GLIBC 2.34、GLIBCXX 3.4.30 或更高，Ubuntu 22.04 满足。`lib/aarch64`、`lib/x86_64` 和 `thirdparty/lib/` 下的同名目录各放一套库，CMake 按当前编译器的目标架构选择。

下文路径都相对于拿到的 `dobot-sdk` 目录。

## 0. 互通网络

RPC 和 DDS 是两条独立的路。`ping` 通只说明 ICMP 能到机器人，不能代替后面的两步。

| 通道 | 用什么地址 | 传输 | 不通时的现象 |
| --- | --- | --- | --- |
| RPC | `SessionConfig.robot_ip`，留空则用上表的默认 IP | TCP，端口 `51234` | `RPC 失败 connect` 或 `no_response` |
| DDS | 不用机器人 IP。看 domain 和网卡 | 电脑上走 UDP；程序跑在机器人本机时还可以走共享内存 | `matched` 一直是 0，`Latest()` 没有数据 |

### 0.1 电脑连机器人

电脑和机器人要在同一网段，中间不要隔着会丢掉 UDP 组播的路由。DDS 发现用组播 `239.255.0.1`。当前 `SessionConfig` 不能填写对端 IP 列表，组播被禁时，RPC 可以正常，DDS 匹配数会一直是 0。

domain 为 0 时，Fast DDS 的端口从 7400 起：组播元流量 7400，单播元流量从 7410 起，用户数据从 7411 起。参与者增多时端口往后加。防火墙要放行接机器人那块网卡上的 **TCP 51234**，以及这块网卡上的 **UDP 7400–7500**。只放行 TCP 时，`GetFsmId` 能成功，状态订阅没有数据。

多网卡时（有线、Wi-Fi、Docker 虚拟网卡同时存在），指定接机器人的那块。网卡名用下面的命令看：

```bash
ip -br link

```

填 `eth0`、`enp3s0` 这种接口名。

按这个顺序确认，在 `dobot-sdk` 目录下执行。x86\_64 电脑用 `example/bin/x86_64/`，机器人本机用 `example/bin/aarch64/`。

1.  看默认地址通不通。
    
    ```bash
    ping -c 3 192.168.5.2
    
    ```
    
    不通就改用机器人的实际地址，后面所有命令加上 `--ip`。
    
2.  看 RPC 端口。
    
    ```bash
    timeout 2 bash -c 'cat < /dev/null > /dev/tcp/192.168.5.2/51234' && echo "port 51234 reachable"
    
    ```
    
3.  查一次当前状态机。成功时打印编号和短名称，`0` 也是合法编号。
    
    ```bash
    ./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2
    
    ```
    
4.  订下肢和上肢状态。成功时先看到 `matched lower=` 和 `upper=` 大于 0，再打印关节角。
    

```bash
./example/bin/x86_64/sr_3588_dds_echo --nic eth0

```

domain 不是 0 时加 `--domain`。只有一块网卡时可以不写 `--nic`。默认等 5 秒，可以加 `--seconds`。

某一架构的 `example/bin/` 是空的，说明那一套还没放进来，换已经放好的架构。

### 0.2 程序跑在机器人本机

RPC 连 `127.0.0.1:51234`，DDS 和本机其它进程发现。预编译程序加 `--on-robot`：

```bash
./example/bin/aarch64/sr_3588_motion_demo --on-robot
./example/bin/aarch64/sr_3588_dds_echo --on-robot

```

自己的代码里把 `config.where` 设为 `dobot::sdk::RunWhere::kRobot`，并且不要填 `robot_ip`。填了 `robot_ip` 就会连那个地址，不再自动改成 `127.0.0.1`。

### 0.3 代码里的连接参数

后面的示例都从这里开始。`robot_ip` 只给 RPC。`network_interface` 只给 DDS，留空表示所有网卡都参与发现。`rpc_port` 留 0 表示用机型默认端口 51234。`async_subscribe` 默认 false：订阅回调在 DDS 监听线程里执行。设为 true 时，回调改到有序 worker。周期状态读 `Latest()`，不必为了回调改这个开关。

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

`Init` 同时打开 RPC 和 DDS。重复 `Init` 返回 false，要先 `Shutdown`。`Shutdown` 等正在执行的回调结束，再释放这一条会话，不要求机器人停机。一个 `Session` 连一台机器人。要连两台，就建两个 `Session`。

`Init` 会在系统临时目录写一份本次的 DDS 配置，会话建立后删除。临时目录不可写时，`Init` 失败。

## 1. 订阅话题，不设置回调

周期状态在自己的线程里读 `Latest()`。还没有数据时返回空，不会把缺失变成数值 0。`WaitForData` 等到第一帧或超时；已经有数据时立刻返回 true。`Matched()` 是当前匹配上的对端数量，0 表示还没发现发布者。

同一 `PubSubType` 只能订一个话题。已经订了 `rt/pelvis/imu/data` 之后，再订 `rt/torso/imu/data` 会失败，因为两路都是 `IMUState_`。`Subscribe` 失败时 `valid()` 为 false。

下面订下肢和上肢状态，等到两路都有数据后各打印前 4 个关节。

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

模板参数是 `PubSubType`，例如 `LowerState_PubSubType`，返回的是 `Subscriber<LowerState_>`。消息类型在命名空间 `dobot::msg::dds_`。头文件搜索路径已经包含 `include/dobot/idl`，包含时写文件名即可。

`robot_type` 是长度 16 的字符数组，读到 `'\0'` 停止。控制环里反复调用 `Latest()`，取到的是当前最新一帧。

包内对照源码：`example/src/sr_3588/dds_echo.cpp`。

## 2. 订阅话题并设置回调

`On()` 用来接急停、故障、触摸这类事件。每来一帧，SDK 先更新 `Latest()`，再调用回调。`async_subscribe` 为 false 时，回调在 Fast DDS 监听线程里、取出样本后立刻执行。设为 true 时，回调改到有序队列的 worker 上。

回调里可以做的事要短：

*   可以读取这一帧，或做一次原子计数，把数据交给自己的线程。
    
*   不要调用 `Shutdown`，不要再 `Subscribe` / `Publish`，不要调用 RPC，也不要在里面睡眠或做长时间计算。
    
*   回调抛出的异常只停这一次调用，后面的帧还会继续到。
    

关节角这种周期数据用第 1 节的 `Latest()`。若只想统计帧率，回调里只做加一，打印放在主线程。包内 `example/src/sr_3588/upper_state_rate.cpp` 是这种写法。

下面订 `Mcu/Estop`。`stop1`、`stop2` 为 0 表示急停，1 表示正常。

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

`On()` 可以在 `Subscribe` 之后再设置。设置前已经到达的帧仍会留在 `Latest()` 里，但不会补进回调。拷贝出来的 `Subscriber` 看的是同一条订阅。

包内对照源码：`example/src/sr_3588/dds_event.cpp`。

## 3. 发布数据

`Publish` 的模板参数同样是 `PubSubType`。一条话题只建一个发布端，控制环里反复 `Write`。新的一帧盖住旧的一帧，命令不排队。

`Write` 返回 true 只表示这一帧交给了本机 DDS，不表示机器人已经执行。先看 `Matched()` 大于 0，再写。

关节命令会作用到电机。发送前确认机器人正处于会执行该话题的状态。下面的例子先订 `rt/upper/state`，再把刚读到的角度原样写回 `rt/upper/cmd`，用来演示发布。它不规划新的轨迹。没有读到状态，或发布端没有匹配上时，例子不发送。

`mode` 取 11，`kp` 取 10，`kd` 取 1，与包内 `upper_arm_swing` 示例一致。IDL 没有给出 `mode` 的取值表。真正要动关节时，先读 `example/src/sr_3588/upper_arm_swing.cpp`：它要求机器人已经处于会执行 `rt/upper/cmd` 的使能状态，并用 `--move` 才会持续发送。

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

控制环里保留这个 `writer`，每个周期改 `q` 后再 `Write`。进程退出后不再有新的关节帧。若同时还在用 RPC 发速度，退出前调用 `session.motion().SetVelocity(0, 0, 0)`。

包里的 `sr_3588_dds_publish_cmd` 会向 `rt/lower/cmd` 发一帧全 0，只有加上 `--send` 才会发送。那是通信匹配测试，日常发布不要从全 0 命令开始。

## 4. 可以发布和订阅的话题

方向按调用方写：**订阅**表示机器人在发，调用方读；**发布**表示调用方在发，机器人收。常量名两台相同。

消息类型都在 `dobot::msg::dds_`。订阅和发布时在类型名后加 `_PubSubType`。字段的生成访问函数与字段同名，例如 `motor.q()` 读取，`motor.q(value)` 写入。

QoS 编译在库里，按「话题名.reader」和「话题名.writer」选取，调用方不用自己填。库里有条目的话题，可靠性和历史深度如下。库里没有条目的话题仍能创建，日志里会有 `QoS profile ... not found`，然后使用 Fast DDS 默认 QoS（RELIABLE、KEEP\_LAST、深度 1）。对端若是 BEST\_EFFORT，这种默认读端或写端会匹配不上，`Matched()` 保持 0。

历史都是 KEEP\_LAST、VOLATILE。下表「QoS」列里的深度是保留的样本数。

| 方向 | 话题 | 常量 | 类型 | QoS | 说明 |
| --- | --- | --- | --- | --- | --- |
| 订阅 | `rt/lower/state` | `kTopicLowerState` | `LowerState_` | BEST\_EFFORT，深度 2 | 下肢关节反馈，15 路 |
| 订阅 | `rt/upper/state` | `kTopicUpperState` | `UpperState_` | BEST\_EFFORT，深度 2 | 上肢关节反馈，18 路，带机型名和上肢是否受控 |
| 发布 | `rt/lower/cmd` | `kTopicLowerCmd` | `LowerCmd_` | BEST\_EFFORT，深度 2 | 下肢关节命令，12 路 |
| 发布 | `rt/upper/cmd` | `kTopicUpperCmd` | `UpperCmd_` | BEST\_EFFORT，深度 2 | 上肢关节命令，18 路 |
| 发布 | `rt/stand/cmd` | `kTopicStandCmd` | `StandingControl_` | 库内无单独条目 | Mini 站立（状态机 300）下的头偏航、腰偏航、身高 |
| 订阅 | `rt/pelvis/imu/data` | `kTopicPelvisImu` | `IMUState_` | BEST\_EFFORT，深度 4 | 骨盆 IMU，`imu_id` 为 1 |
| 订阅 | `rt/torso/imu/data` | `kTopicTorsoImu` | `IMUState_` | BEST\_EFFORT，深度 4 | 躯干 IMU，`imu_id` 为 2。与骨盆 IMU 不能同时订 |
| 订阅 | `rt/gamepad/state` | `kTopicGamepadState` | `GamepadState_` | BEST\_EFFORT，深度 4 | 手柄按键和摇杆 |
| 订阅 | `rt/main/nodes/state` | `kTopicMainNodesState` | `MainNodesState_` | BEST\_EFFORT，深度 2 | 各伺服节点和 EtherCAT 从站 |
| 订阅 | `Mcu/BmsCommon` | `kTopicMcuBmsCommon` | `BmsCommon_` | RELIABLE，深度 1 | 电池常用遥测，电量看 `remaining_pct` |
| 订阅 | `Mcu/BmsUncommon` | `kTopicMcuBmsUncommon` | `BmsUncommon_` | RELIABLE，深度 1 | 逐节电压、电芯温度、版本 |
| 订阅 | `Mcu/Heartbeat` | `kTopicMcuHeartbeat` | `Heartbeat_` | 库内无单独条目 | MCU 心跳序号 |
| 订阅 | `Mcu/Estop` | `kTopicMcuEstop` | `Estop_` | RELIABLE，深度 1 | 两路急停。0 急停，1 正常 |
| 订阅 | `Mcu/Adc` | `kTopicMcuAdc` | `Adc_` | 库内无单独条目 | MCU 电源 ADC 原始码 |
| 订阅 | `Mcu/Temp` | `kTopicMcuTemp` | `Temp_` | 库内无单独条目 | 板载温度，0.01°C/格 |
| 订阅 | `Mcu/Init` | `kTopicMcuInit` | `McuInit_` | RELIABLE，深度 1 | MCU 上电信息：网络、IP、运行时间、固件版本 |
| 订阅 | `Mcu/Fault` | `kTopicMcuFault` | `Fault_` | 库内无单独条目 | 尚未消除的 MCU 故障码 |
| 发布 | `rt/enable/motors` | `kTopicEnableMotors` | `EnableMotors_` | RELIABLE，深度 1 | 按关节开关使能，`flag[30]` |
| 发布 | `rt/clear/errors` | `kTopicClearErrors` | `ClearErrors_` | RELIABLE，深度 1 | 请求清除驱动错误 |
| 发布 | `rt/go/home` | `kTopicGoHome` | `GoHome_` | RELIABLE，深度 1 | 按关节回零，`flag[30]` |
| 发布 | `rt/set/calibration/flag` | `kTopicSetCalibrationFlag` | `SetCalibrationFlag_` | RELIABLE，深度 1 | 按关节设置标零标志，`flag[30]` |
| 订阅 | `rt/set/fsm/id` | `kTopicSetFsmId` | `SetFsmId_` | RELIABLE，深度 1 | 运控算法发布。空或 `"\0"` 不可播放，`"enable"` 可播放但未在播，其它字符串是正在播放的技能名。改状态机用 `session.motion()` |
| 发布 | `rt/remote/control` | `kTopicRemoteControl` | `RemoteControl_` | BEST\_EFFORT，深度 2 | 遥控摇杆。左摇杆平移，右摇杆转向 |
| 订阅 | `rt/diagnostic/report` | `kTopicDiagnosticReport` | `DiagnosticReport_` | RELIABLE，深度 1 | 电机、IMU、手柄、性能、运控、头、灯、网络、串口、主节点的汇总诊断 |
| 订阅 | `rt/locomotion/alarm/state` | `kTopicLocomotionAlarm` | `LocomotionAlarmState_` | 库内无单独条目 | 运控尚未消除的告警 |
| 订阅 | `rt/wifi/status` | `kTopicWifiStatus` | `WifiStatus_` | 库内无单独条目 | Wi-Fi 配网心跳、模式、IP、故障 |
| 订阅 | `dobot_perf_fault` | `kTopicPerfFault` | `FaultInfo_` | 库内无单独条目 | 性能监测故障或恢复。话题名没有 `rt/` 前缀 |
| 订阅 | `rt/voice/command` | `kTopicVoiceCommand` | `VoiceCommand_` | 库内无单独条目 | 语音识别结果 |
| 发布 | `rt/voice/tts_request` | `kTopicVoiceTtsRequest` | `TTSRequest_` | 库内无单独条目 | 请求合成并播报文本 |
| 发布 | `rt/voice/ack_request` | `kTopicVoiceAckRequest` | `AckRequest_` | 库内无单独条目 | 请求播放已有的固定音频 |
| 订阅 | `rt/voice/output_event` | `kTopicVoiceOutputEvent` | `VoiceOutputEvent_` | 库内无单独条目 | 播报是否被接受，以及开始、结束、打断、失败 |
| 发布 | `rt/head/eye_effect_cmd` | `kTopicHeadEyeEffectCmd` | `HeadEyeEffectCmd_` | RELIABLE，深度 1 | 设置眼部表情 |
| 发布 | `rt/head/info_request` | `kTopicHeadInfoRequest` | `HeadInfoRequest_` | RELIABLE，深度 1 | 向头部要一次设备信息 |
| 订阅 | `rt/head/state` | `kTopicHeadState` | `HeadState_` | BEST\_EFFORT，深度 2 | 头部对信息请求的回复，不是周期状态 |
| 订阅 | `rt/head/touch_event` | `kTopicHeadTouchEvent` | `HeadTouchEvent_` | BEST\_EFFORT，深度 2 | 头部触摸 |
| 订阅 | `rt/head/error_event` | `kTopicHeadErrorEvent` | `HeadErrorEvent_` | BEST\_EFFORT，深度 2 | 头部错误 |

`include/dobot/idl/` 里还有其它生成类型。没有出现在 `topics.hpp` 里的类型，本 SDK 没有给出话题名，不要自己猜话题字符串。

头文件和类型的对应关系：

| 类型 | 头文件 |
| --- | --- |
| `LowerState_` | `lower_state.hpp` |
| `UpperState_` | `upper_state.hpp` |
| `LowerCmd_` | `lower_cmd.hpp` |
| `UpperCmd_` | `upper_cmd.hpp` |
| `StandingControl_` | `standing_control.hpp` |
| `IMUState_` | `imu_state.hpp` |
| `GamepadState_` | `gamepad_state.hpp` |
| `MainNodesState_` | `main_nodes_state.hpp` |
| `BmsCommon_`、`BmsUncommon_`、`Heartbeat_`、`Estop_`、`Adc_`、`Temp_`、`McuInit_`、`Fault_` | `mcu_data.hpp` |
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
| `VoiceCommand_`、`TTSRequest_`、`AckRequest_`、`VoiceOutputEvent_` | `voice.hpp` |
| `HeadEyeEffectCmd_`、`HeadInfoRequest_`、`HeadState_`、`HeadTouchEvent_`、`HeadErrorEvent_` | `head_control.hpp` |

每个头文件旁边都有同名的 `PubSubTypes.hpp`，两个都要包含。逐字段结构见同目录的 [DDS 话题说明](./dds-topics.zh-CN.md)。下面只写调用时必须知道的约定。

### 4.1 关节状态和命令

`MotorState_` 用来反馈：`mode`、`q`、`dq`、`ddq`、`tau_est`、`tau_cmd`、电流、原始值、三路温度、`bus_voltage`。`timestamp_us` 在 `LowerState_` 和 `UpperState_` 上，单位微秒。`MotorCmd_` 用来下发：`mode`、`q`、`dq`、`tau`、`kp`、`kd`。

IDL 没有标注 `q`、`dq`、力矩和温度的单位。包内上肢摆臂示例按弧度填写 `q`。

下标约定：

| 数据 | 长度 | 下标 |
| --- | --- | --- |
| `rt/lower/cmd` | 12 | 0–5 左腿，6–11 右腿 |
| `rt/lower/state` | 15 | 前 12 路与下肢命令对齐。多出的 3 路含义没有写明 |
| `rt/upper/state` 和 `rt/upper/cmd` | 18 | 0–2 腰，3–9 左臂，10–16 右臂，17 头 |

每一路的关节名和正方向没有在 IDL 里逐项定义。摆臂示例把上肢下标 4 和 11 当作左右臂向外摆的那一路。

`UpperState_.is_upper_control` 为 true 表示上肢当前有控制。`robot_type` 是长度 16 的字符数组，取值表没有在 IDL 里列出。

`rt/stand/cmd` 的三个 float 同时生效：`head_yaw` 是头部偏航绝对目标，单位 rad；`waist_yaw` 是相对默认姿态的腰偏航偏移，单位 rad；`body_height` 是身体高度目标，单位 m，0 恢复默认站立高度。腰的俯仰和横滚不在这里。发送前先用 `session.motion().SetRobotMode(dobot::motion::RobotMode::kStanding)` 进入 Mini 站立。这条话题在库内没有单独 QoS，匹配数为 0 时先看日志里的 profile 警告。

### 4.2 IMU、手柄、主节点

两路 IMU 字段相同：`quaternion[4]`、`gyroscope[3]`、`accelerometer[3]`、`rpy[3]`（滚转、俯仰、偏航）、`temperature`、`timestamp_us`。四元数顺序和物理单位 IDL 没有标注。一次会话只能订其中一路。骨盆这一路会周期发布；躯干这一路的发布函数存在，当前机载主循环没有调用。

手柄 `btn_state` 的 bit：

| bit | 键 | bit | 键 |
| --- | --- | --- | --- |
| 0 | R1 | 8 | A |
| 1 | L1 | 9 | B |
| 2 | start | 10 | X |
| 3 | select | 11 | Y |
| 4 | R2 | 12 | up |
| 5 | L2 | 13 | right |
| 6 | F1 | 14 | down |
| 7 | F2 | 15 | left |

`lx`、`ly`、`rx`、`ry` 是左右摇杆。数值范围 IDL 没有标注。

`MainNodesState_` 含左腿 6、右腿 6、腰 3、左臂 7、右臂 7、头 2，以及两路 EtherCAT 从站。每个轴上有伺服状态、错误码、警告码、温度、母线电压、软件版本和标零结果。`zero_calib`：0 未成功，1 成功，2 未知。

### 4.3 电池和 MCU

电量百分比是 `BmsCommon_.remaining_pct`，0–100。`current_ma` 单位 mA，充电为负。电芯和 MOS 温度字段以 `_x10` 结尾的是 0.1°C/格。电压字段里，`max_cell_mv`、`min_cell_mv` 单位 mV；带 `_10mv` 的是 10 mV/格。

`Mcu/Temp` 的 `local_temp`、`remote_temp` 是摄氏度乘 100。`Mcu/Adc` 是原始 ADC 码，换算关系不在 IDL 里。`Mcu/Fault` 的有效故障码个数看 `dtc_count`，列表容量 32。`Mcu/Init` 的 `network_ok` 为 1 表示 MCU 网络已经起来，`version` 最长 32 字符。

### 4.4 运控命令话题

`EnableMotors_`、`GoHome_`、`SetCalibrationFlag_` 都是 `flag[30]`。30 路与具体关节的对应顺序，IDL 没有写明。

`ClearErrors_.msg_id` 用来标识这一次请求。

`rt/set/fsm/id` 由运控算法发布，调用方订阅。`SetFsmId_` 带 `id` 和 `current_action`。`current_action` 为空或 `"\0"` 表示不可播放，`"enable"` 表示可播放但没在播，其它非空字符串是正在播放的技能名。改状态机用第 5 节的 `session.motion()`，不要往这个话题写。包内 `example/src/sr_3588/action_watch.cpp` 订阅这一路，用来看技能开始和结束。

`RemoteControl_` 的 `btn_move` 是左摇杆，`btn_turn` 是右摇杆，各有 `x`、`y`。摇杆范围和正方向 IDL 没有标注。速度来源的优先级是手柄高于 App，App 高于 `SetVelocity`。低优先级要等高优先级 0.5 秒没有新指令。

### 4.5 诊断、告警、Wi-Fi、性能

`DiagnosticReport_` 的每个分块有自己的 `data_valid`。为 false 时不要使用该分块。字段很多，以 `diagnostic_service.hpp` 为准。

`LocomotionAlarmState_.active_alarms` 最多 256 条。`truncated` 为 true 表示列表被截断。严重级别：1 警告，2 错误，3 严重。来源：1 轴，2 急停，3 IMU，4 起身，5 升降，6 下肢命令，7 上肢命令，8 手部命令，9 启动，0 未知。

`WifiStatus_.event_type`：0 心跳，1 配网开始，2 成功，3 失败，4 IP 变化，5 故障，6 退出。`work_mode`：0 idle，1 station，2 ap。`seq` 单调递增。

`FaultInfo_.is_recovered` 为 true 表示这是一次恢复。`fault_level`：0 到 3 对应 S0 到 S3。

### 4.6 语音

四条消息都带 `header`：`source` 是来源模块名，`timestamp` 是 Unix 毫秒。

识别结果 `VoiceCommand_`：`is_final` 区分中间结果和最终结果，`confidence` 为 0.0–1.0，`language` 无法确定时为 `auto`。`command_id` 是这一次识别的标识。

播报请求 `TTSRequest_`：`speak_id` 用调用方生成的 UUID，`text` 是待合成文本。长文本按 `seq_num` 分片，从 0 开始，最后一片 `is_final` 为 true。`interrupt` 为 true 时打断当前播报。回复某次识别时，把对应的 `command_id` 填进 `reply_to_command_id`。优先级从低到高：`PRIORITY_NORMAL`、`PRIORITY_IMPORTANT`、`PRIORITY_SYSTEM`、`PRIORITY_EMERGENCY`。

`AckRequest_` 播放已经存在的固定音频。`ack_id` 是音频资产 ID，不传文件路径，也不传文本。

结果在 `rt/voice/output_event`。程序分支看 `reason`，`detail` 只用于日志。`has_seq_num` 为 false 时忽略 `seq_num`，ACK 和整段播放生命周期事件属于这种情况。`event_type` 包括 `REQUEST_BUFFERED`、`REQUEST_ACCEPTED`、`REQUEST_REJECTED`、`REQUEST_DROPPED`、`PLAYBACK_STARTED`、`PLAYBACK_COMPLETED`、`PLAYBACK_INTERRUPTED`、`PLAYBACK_FAILED`。

### 4.7 头部

`rt/head/state` 不是周期话题。先发布 `rt/head/info_request`，头部才会回复一次 `rt/head/state`。触摸和错误是事件，用 `On()`。

`eye_effect` 取值：

| 值 | 名称 | 值 | 名称 |
| --- | --- | --- | --- |
| `0x01` | ANGRY | `0x0A` | SERIOUS |
| `0x02` | BASE | `0x0B` | TIRED |
| `0x03` | CAREFUL | `0x0C` | WAKEUP |
| `0x04` | DAZE | `0x0D` | WINK1 |
| `0x05` | DEYI | `0x0E` | YUNTOU |
| `0x06` | GANGA | `0x0F` | ZHENWANG |
| `0x07` | HUAIXIAO | `0x10` | WINK2 |
| `0x08` | JIANDING | `0x11` | GLANCE2 |
| `0x09` | MOON | `0xFF` | 启用 `color` |

`eye_effect` 为 `0xFF` 时，`color` 才启用。`color` 是 RGB888，低 24 位有效。`duration_ms` 为 0 表示用 MCU 的默认保持时间。`control_node_id` 只作记录，头部不据此选路。

触摸区域：`HEAD_TOUCH_TOP`、`HEAD_TOUCH_LEFT_0`、`HEAD_TOUCH_LEFT_1`、`HEAD_TOUCH_RIGHT_0`、`HEAD_TOUCH_RIGHT_1`。`pressed` 可以同时包含多个区域。

## 5. 如何使用 RPC 接口

日常运控走 `session.motion()`。调用是阻塞的：连上、发出一条 JSON-RPC 2.0、读到响应或超时后返回。默认超时 3000 毫秒，覆盖连接、发送和接收。每次调用新建一条 TCP，调用方不用保活连接。成功返回 `0`，否则返回与下面错误码表一致的整型值（同 `RpcError`）。`GetFsmId` 写入当前状态机编号，`0` 也合法。`GetFsmList` 写入 `std::vector<dobot::motion::FsmInfo>`。`FsmIdToString` 把编号翻成短字符串。

预编译程序 `sr_3588_motion_demo` 不带参数时查询当前状态机。切模式加 `--mode`，预设动作加 `--action`，舞蹈加 `--dance`。`--fsm-id` 能对应到这些接口时走对应函数，其余编号走 `session.rpc().SetFsmId`。速度示例是 `sr_3588_walk_demo`，必须加 `--run`。

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

`SetRobotMode` 的映射：`kZeroTorque` 0，`kDamping` 1，`kCustomDebug` 2，`kReady` 100，`kStanding` 300，`kWalking` 304。进入 Mini 站立之后才能发 `rt/stand/cmd`。

预设动作：`AutoEnterBox`（307）、`LieDown`（310）、`GoProne`（312）完成后运控会自动进入阻尼（1）；`AutoExitBox`（308）、`StandUpFromSupine`（311）、`StandUpFromProne`（313）完成后会先进入拟人行走（304），约 2 秒后再自动进入站立（300）。`AutoExitBox` 返回后不要立刻再切到站立。成功只表示切换被接受。

舞蹈用 `PlayDance(fsm_id)`。`fsm_id` 须在 350–400。不要和上述六个预设混用。已有舞蹈从 `GetFsmList` 读取：`is_dance` 为 true 时还有 `dance_name`、`model_file`、`motion_loop_length`、`motion_hold_back_steps`、`motion_start_step`。

技能目录里的 csv、重新加载技能、下发按键映射仍走 `session.rpc()`。这一层返回 `RpcResult`。先看 `ok()`。`ok()` 为真表示传输成功，且响应里有 `result`。会改机器人状态的方法，机器人返回 `result` 0 时，封装函数把 `ok()` 设为假，并填上业务错误码。`result()` 在没有该字段时抛异常。`error` 用 `RpcErrorName` 打成稳定短字符串，`message` 是说明，`response` 是原始 JSON。`ExecuteAction` 的超时只覆盖这次 RPC，已受理不表示动作播完。

```cpp
const dobot::sdk::RpcResult action = session.rpc().ExecuteAction("skill.csv");
if (!action.ok()) {
    std::cerr << "ExecuteAction failed " << dobot::sdk::RpcErrorName(action.error)
              << ": " << action.message << "\n";
    return 1;
}

```

还没有封装成函数的方法用 `session.Call`。`params` 传 JSON 对象。这和预编译程序 `sr_3588_rpc_call --method ... --params ...` 是同一条路。`GetFsmId`、`SetFsmId`、`SetVel` 已经有语义接口，不要再按方法名调用。

```cpp
const dobot::sdk::RpcResult raw = session.Call("MethodName", nlohmann::json::object(), 3000);

```

错误码：

| 值 | 名称 | 含义 |
| --- | --- | --- |
| 0 | `ok` | 调用成功 |
| 1 | `socket` | 创建套接字失败 |
| 2 | `connect` | 连接失败、IP 无效或连接超时 |
| 3 | `no_response` | 发送失败、接收失败或没有读到响应 |
| 4 | `json` | 响应不是合法 JSON，或 `result` 类型不对 |
| 5 | `unexpected` | 响应里没有 `result`，或只有 JSON-RPC error，或响应超过 1MB。`PlayDance` 的编号超出 350–400 时，语义接口也返回这个码 |
| 6 | `server_internal` | `result` 为 -1，或没有单独错误码的业务失败 |
| 100 | `fsm_transition_failed` | 状态机切换失败 |
| 102 | `set_vel_failed` | 速度指令设置失败 |
| 106 | `execute_action_failed` | 动作文件执行失败 |

101、103、104、105 留在错误码表里，当前没有对应的封装方法。

## 6. RPC 接口有哪些

两台机器人共用 `dobot::sdk::MotionRpc`，声明在 `include/dobot/sdk/motion_rpc.hpp`。型号头文件里只有 IP、端口和话题名，没有方法列表。

`session.motion()` 使用的状态机编号：0 零力矩，1 阻尼，2 底层调试，100 预备站立，300 Mini 站立（进入之后才能发 `rt/stand/cmd`），304 拟人行走，307 自主入箱，308 自主出箱，310 躺下，311 仰姿起身，312 趴下，313 俯姿起身。350 到 400 是舞蹈，用 `PlayDance`。这些编号仍待与机载 `GetFsmId` 核对。舞蹈列表用 `GetFsmList` 读，SDK 没有 `AddDancePolicy`。

| C++ 方法 | JSON 方法 | 参数 | 成功时的 `result` | 说明 |
| --- | --- | --- | --- | --- |
| `GetFsmId()` | `GetFsmId` | 无 | 当前状态机编号，整数。0 也合法 | 查询。`result` 为 -1 时错误码 6 |
| `SetFsmId(fsm_id)` | `SetFsmId` | `fsm_id`：int32 | 1 | 切换失败时 `result` 为 0，错误码 100。300 为 Mini 站立 |
| `GetFsmList()` | `GetFsmList` | 无 | 对象数组 | 每项有 `fsm_id`、`is_dance`、`name`。`is_dance` 为 true 时还有 `dance_name`、`model_file`、`motion_loop_length`、`motion_hold_back_steps`、`motion_start_step`。`result` 不是数组时错误码 4，为 -1 时错误码 6 |
| `SetVel(vx, vy, vyaw, duration)` | `SetVel` | 四个 float。`duration` 默认 1.0 秒 | 1 | `vx` 向前为正，m/s；`vy` 向左为正，m/s；`vyaw` 逆时针为正，rad/s。到时后停止。停住再调用 `SetVel(0, 0, 0)`。失败时错误码 102。手柄和 App 的优先级高于这次 RPC |
| `ReloadArmSkillsData()` | `ReloadArmSkillsData` | 无 | 1 | 重新加载技能列表。调用前把轨迹 csv 放到机器人上的 `/dobot/algs/policies/humanoid/motion_data/mini/skills_user_defined` |
| `SendActionMapping(mapping)` | `SendActionMapping` | `std::map<std::string, std::string>`，键是按键，值是技能名 | 1 | 只覆盖这次出现的键，不合并旧映射 |
| `ExecuteAction(file_path)` | `ExecuteAction` | 技能目录下的文件名 | 1 表示已受理 | 不表示动作播完。超时只覆盖 RPC。失败时错误码 106 |

每个方法最后一个参数都是 `timeout_ms`，默认 3000。语义接口 `session.motion()` 成功返回 0，失败返回同一套错误码。`PlayDance` 的编号不在 350–400 时返回错误码 5。

已经有语义接口的方法，命令行用 `motion_demo` 和 `walk_demo`：

```bash
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2 --mode stand
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2 --action lie
./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2 --dance 350
./example/bin/x86_64/sr_3588_walk_demo --ip 192.168.5.2 --run --vx 0.2 --seconds 1

```

`ReloadArmSkillsData`、`SendActionMapping`、`ExecuteAction` 以及还没有成员函数的方法，才用 `rpc_call`。`--method` 必填。`--params` 的字段名与上表一致：

```bash
./example/bin/x86_64/sr_3588_rpc_call --ip 192.168.5.2 --method ReloadArmSkillsData
./example/bin/x86_64/sr_3588_rpc_call --ip 192.168.5.2 --method ExecuteAction --params '{"file_path":"skill.csv"}'

```

本 SDK 没有封装整机的设备状态、音频、导航、GPIO。错误码表里出现过、当前接口没有的方法（上肢控制开关、上肢关节、升降位姿、轨迹录制）不要按这些名字去 `Call`。查舞蹈列表用 `GetFsmList`，播放用 `session.motion().PlayDance`。

## 7. 接到自己的工程

把整个 `dobot-sdk` 目录放进工程。头文件、`lib/<架构>/libdobot_sdk.a`、`lib/<架构>/libdobot_log.so`、`thirdparty/lib/<架构>/` 里的 `libfastdds.so` 和 `libfastcdr.so` 要在一起。只拷头文件或只拷静态库，链接或运行时会缺库。

两种接法：

| 方式 | 什么时候用 |
| --- | --- |
| `add_subdirectory` | 默认用这个。一份目录里同时有 aarch64 和 x86\_64，CMake 按当前编译器选择。`dobot_sdk` 目标会带上线程库、Fast DDS、Fast CDR 和 `dobot_log` |
| `install` 后再 `find_package` | 只在一种架构上使用，并且希望装到一个前缀里。一次 install 只放入当前这次选中的架构，两套架构要装到两个前缀 |

### 7.1 把目录加进工程

假设自己的工程和 `dobot-sdk` 同级，或者已经把该目录拷到工程里的 `third_party/dobot-sdk`。

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

`BUILD_EXAMPLES` 设为 OFF，避免把 SDK 自带示例也编进去。不设的话，示例会一起编译。

`RPATH` 按可执行文件的实际相对位置改。也可以在运行前指定动态库目录：

```bash
export LD_LIBRARY_PATH=/path/to/dobot-sdk/lib/x86_64:/path/to/dobot-sdk/thirdparty/lib/x86_64

```

缺 `libdobot_log.so`、`libfastdds.so` 或 `libfastcdr.so` 时，程序还没进入 `main` 就会报找不到共享库。

交叉编译示例：

```bash
cmake -S . -B build -DTARGET_ARCH=aarch64 -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
cmake --build build -j

```

系统的 glibc 或 libstdc++ 低于前面的版本时，配置会提示链不上 `libfastdds.so`。`Session::Init` 会启动 DDS，所以自己的程序也要能链上这套库。

### 7.2 安装后再 find\_package

在 `dobot-sdk` 目录执行。这只安装当前编译器对应的那一套，并把它展平到前缀的 `lib/`，不再保留 `aarch64`、`x86_64` 子目录。

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/dobot_sdk
cmake --build build
cmake --install build

```

自己的工程：

```cmake
find_package(dobot_sdk REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dobot_sdk)

```

前缀不在 CMake 默认搜索路径里时：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/dobot_sdk

```

安装出来的 `dobot_sdk` 目标链接了 `fastdds`、`fastcdr` 和线程库。若链接报 `dobot_log` 的符号没有定义，再把前缀里的 `lib/libdobot_log.so` 加进 `target_link_libraries`。运行时把 `$HOME/dobot_sdk/lib` 放进 `RPATH` 或 `LD_LIBRARY_PATH`。

头文件搜索路径两种接法都会带上 `include/` 和 `include/dobot/idl/`。消息头用 `"lower_state.hpp"` 这种写法，SDK 头用 `"dobot/sdk/session.hpp"`。

## 8. 连不上时先查什么

| 现象 | 先看 |
| --- | --- |
| `connect` | IP、端口 51234、本机到该地址的路由。先 `ping`，再测 TCP |
| `no_response` | 端口是不是 51234，超时是不是太短。可以加 `--timeout-ms` 或把 `timeout_ms` 加大 |
| `json` | `--params` 或自己组的 JSON 是不是完整对象 |
| `unexpected` 或 `server_internal` | 方法名是否在第 6 节的表里。把 `message` 和 `response.dump()` 打出来 |
| 错误码 100、102、106 | 网络是通的，机器人拒绝了这次业务调用 |
| RPC 成功，DDS `Matched()` 一直是 0 | domain 是否为 0，`network_interface` 是否是接机器人的那块网卡，防火墙是否放行 UDP 7400–7500，中间网络是否允许组播 `239.255.0.1` |
| 日志里有 `QoS profile ... not found` | 这个话题在第 4 节标了「库内无单独条目」。对端若是 BEST\_EFFORT，默认 RELIABLE 匹配不上 |
| 能订到状态，命令没有动作 | 当前状态机是否会消费这条话题。`rt/stand/cmd` 要先 `SetRobotMode(kStanding)`。`Write` 成功只表示本机发出 |
| 启动即报缺少 `libdobot_log.so` 或 `libfastdds.so` | `LD_LIBRARY_PATH` 或 `RPATH` 是否包含 `lib/<架构>` 和 `thirdparty/lib/<架构>` |
| 配置时提示链不上 `libfastdds.so` | 系统 GLIBC 低于 2.34，或 GLIBCXX 低于 3.4.30，或编译器架构和 `lib/` 子目录不一致 |

包内可以对照的源码在 `example/src/sr_3588/` 和 `example/src/sr_thor/`。公开包的 CMake 会编译 `motion_demo`、`walk_demo`、`rpc_call`、`dds_echo`、`dds_publish_cmd`、`upper_state_rate`、`upper_arm_swing`。同目录里还有 `dds_event.cpp`、`action_watch.cpp`，公开包的 CMake 没有编进去。`action_watch` 订阅 `rt/set/fsm/id`。查状态机和切模式用 `motion_demo`，速度用 `walk_demo`。`rpc_call` 只用于还没有单独示例的方法。用法以本文第 2、5、6 节的代码为准。