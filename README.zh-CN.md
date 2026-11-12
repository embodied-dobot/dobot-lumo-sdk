# dobot-sdk

[English](./README.md) | **简体中文**

用于控制和监测 Dobot 人形机器人（SR_3588、SR_thor）的 C++ SDK，通过 JSON-RPC 和 Fast DDS，可在电脑上经网络连接机器人，也可在机器人本机运行。

本目录包含公开头文件、预编译库和已编好的示例，不含 SDK 的实现源码。

> **项目状态：** 早期版本（0.2.0）。主版本之间 API 可能变化；增加函数或字段属于次版本，被废弃的函数会保留一个版本后再删除。

## 支持的机器人

两个型号的 RPC 方法和 DDS 话题相同，头文件各一份。

| 型号 | 命名空间 | 默认 IP | RPC 端口 | DDS domain |
|---|---|---|---|---|
| SR_3588 | `dobot::robot::sr_3588` | `192.168.5.2` | `51234` | `0` |
| SR_thor | `dobot::robot::sr_thor` | `172.16.20.2` | `51234` | `0` |

## 快速开始

### 环境要求

- x86_64 或 aarch64 的 Linux，GLIBC 2.34、GLIBCXX 3.4.30 或更高（Ubuntu 22.04 满足）
- C++17 编译器和 CMake 3.16 或更高（仅在重编示例或接入自己的工程时需要）
- 能访问机器人 IP 的网络

预编译库在 `lib/<架构>` 和 `thirdparty/lib/<架构>`，CMake 按当前编译器的目标架构自动选择。

### 运行已编好的示例

下面以 SR_3588 为例。SR_thor 把程序名里的 `sr_3588` 换成 `sr_thor`，IP 用 `172.16.20.2`。

1. 确认网络能通：

   ```bash
   ping -c 3 192.168.5.2
   ```

   机器人不在默认地址时，后面的命令用 `--ip` 填实际地址。

2. 查询当前状态机编号。电脑用 `example/bin/x86_64`，机器人用 `example/bin/aarch64`：

   ```bash
   # 在电脑上
   ./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2

   # 在机器人本机
   ./example/bin/aarch64/sr_3588_motion_demo --on-robot
   ```

预期输出（打印当前状态机编号和短名称，`0` 也是合法编号）：

```text
Current FSM 300 (standing)
```

连不上时会打印 `GetFsmId failed` 和错误名。请核对 IP、端口 `51234` 以及本机到机器人的路由。默认超时 3000 毫秒，可用 `--timeout-ms` 修改。

常用参数：

| 参数 | 含义 |
|---|---|
| `--on-robot` | RPC 连本机 `127.0.0.1`（程序在机器人上时） |
| `--ip <地址>` | 机器人地址（程序在电脑上时） |
| `--nic <网卡名>` | DDS 发现使用的网卡；电脑有多块网卡时填接机器人的那块 |
| `--domain <id>` | DDS domain 不是 `0` 时填写 |
| `--timeout-ms <毫秒>` | RPC 超时 |

### 其他示例

程序名为 `sr_3588_<名称>` / `sr_thor_<名称>`，源码在 `example/src/<型号>/`。

| 示例 | 作用 |
|---|---|
| `motion_demo` | 查询状态机编号。`--mode stand` 切到 Mini 站立，`--action` 预设动作，`--dance` 舞蹈 |
| `walk_demo` | 前后走（需加 `--run`） |
| `rpc_call` | 发送原始 JSON-RPC（必须带 `--method`），用于技能重载、动作文件和尚未封装的方法 |
| `dds_echo` | 订阅下肢 / 上肢状态 |
| `dds_publish_cmd` | 发一帧下肢命令（需加 `--send`） |
| `dds_event` | 订阅急停话题并进入回调 |
| `tts_demo` | 向语音服务发文本，并等待播放结果 |
| `action_watch` | 技能开始和结束时打印（`--all` 打印每一帧） |
| `state_gap`、`state_gap_pro`、`upper_state_rate`、`upper_arm_swing` | 状态频率与间隔检测，以及上肢摆臂示例 |

例如在机器人上直接调用 RPC 方法：

```bash
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method ReloadArmSkillsData
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method ExecuteAction --params '{"file_path":"skill.csv"}'
```

> [!WARNING]
> `walk_demo`、`dds_publish_cmd` 以及动作类示例会让真机运动。运行前请确认机器人周围无人和障碍物，并保证急停触手可及。

### 重编示例

```bash
cmake -S . -B build
cmake --build build -j
```

程序在 `build/bin/`。交叉编译时指定目标架构的编译器，例如 `-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++`，CMake 会改用 `lib/aarch64` 和 `thirdparty/lib/aarch64`。

系统的 glibc 或 libstdc++ 低于要求时，配置会提示链不上 `libfastdds.so`，并跳过 DDS 示例；`motion_demo`、`walk_demo` 和 `rpc_call` 仍会编出来。

## 接到自己的工程

安装后再 `find_package`：

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/dobot_sdk
cmake --build build
cmake --install build
```

```cmake
find_package(dobot_sdk REQUIRED)
target_link_libraries(my_app PRIVATE dobot_sdk)
```

安装前缀不在 CMake 默认搜索路径里时，配置自己的工程加上 `-DCMAKE_PREFIX_PATH=/opt/dobot_sdk`。

也可以不安装，直接把本目录加进工程：

```cmake
add_subdirectory(dobot-sdk)
target_link_libraries(my_app PRIVATE dobot_sdk)
```

这样会把示例也编进去，不需要时加 `-DBUILD_EXAMPLES=OFF`。当前系统能链上 Fast DDS 时，`dobot_sdk` 会带上 `fastdds` 和 `fastcdr`。

应用通过 `dobot::sdk::Session` 连接机器人。DDS 的 domain 和网卡用 `SessionConfig` 的 `domain_id`、`network_interface` 设置。

## 目录结构

| 路径 | 内容 |
|---|---|
| `include/dobot/sdk/` | DDS 会话、JSON-RPC 传输 |
| `include/dobot/robot/sr_3588/`、`include/dobot/robot/sr_thor/` | 各型号的话题名、RPC 方法和默认 IP |
| `include/dobot/idl/` | DDS 消息头文件 |
| `include/nlohmann/` | JSON 头文件 |
| `lib/<架构>/` | `libdobot_sdk.a`、`libdobot_log.so` |
| `thirdparty/lib/<架构>/`、`thirdparty/include/` | Fast DDS、Fast CDR 的库和头文件 |
| `example/bin/<架构>/` | 已编好的示例 |
| `example/src/<型号>/` | 示例源码 |

## 文档

- [使用指南](./docs/user-guide.zh-CN.md) — 网络互通、订阅与发布、可用话题列表。
- [DDS 话题说明](./docs/dds-topics.zh-CN.md) — 各 DDS 话题的字段说明。

## 支持与联系

- **缺陷反馈与功能建议：** {{ISSUE_TRACKER_URL}}
- **维护者：** {{MAINTAINER_NAME_OR_EMAIL}}

提交缺陷反馈时，请提供 SDK 版本（`GetSdkVersion()`）、机器人型号、操作系统与架构、复现步骤，以及预期行为和实际行为。

## 安全

请勿通过公开 Issue 报告安全漏洞，请私下发送到 {{SECURITY_CONTACT}}。

## 许可证

{{LICENSE_NAME}}，详见 [LICENSE](./LICENSE)。
