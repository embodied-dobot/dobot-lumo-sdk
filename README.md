# dobot-sdk

在自己的电脑上通过网络连接机器人，或拷到机器人本机上运行的 C++ SDK。这个目录里是头文件、预编译库和已经编好的示例。

需要 C++17 和 CMake 3.16 或更高版本。预编译库要求系统提供 GLIBC 2.34 和 GLIBCXX 3.4.30 或更高版本，Ubuntu 22.04 满足。`lib/<架构>` 和 `thirdparty/lib/<架构>` 按 aarch64、x86_64 各放一套。CMake 按当前编译器的目标架构自动选择。

## 型号

两台的 RPC 方法和 DDS 话题相同，头文件各自一份。运控 RPC 端口都是 `51234`，DDS domain 默认是 `0`。

| 型号 | 命名空间 | 默认 IP |
|---|---|---|
| SR_3588 | `dobot::robot::sr_3588` | `192.168.5.2` |
| SR_thro | `dobot::robot::sr_thro` | `172.16.20.2` |

下面用 SR_3588 走通一次查询。SR_thro 把程序名里的 `sr_3588` 换成 `sr_thro`，IP 换成 `172.16.20.2`。

## 10 分钟跑通 GetFsmId

### 1. 确认网络

调用方要能访问机器人 IP。先看默认地址通不通：

```bash
ping -c 3 192.168.5.2
```

不通就改用机器人的实际地址，后面运行示例时加上 `--ip`。

`GetFsmId` 走 TCP，端口 `51234`。这一步只要路由能到达该 IP，不用填网卡名。

订 DDS 状态时，domain 默认是 `0`，所有网卡都参与发现。电脑上有多块网卡时，给示例加上 `--nic`，填接机器人的那块。domain 不是 `0` 时加 `--domain`。

### 2. 运行已经编好的示例

在本目录或 `example/bin/<架构>/` 里执行都可以。`example/bin/aarch64` 给机器人，`example/bin/x86_64` 给 x86_64 电脑。程序从包根的 `lib/<架构>` 找 `libdobot_log.so`，从 `thirdparty/lib/<架构>` 找 Fast DDS。

机器人上：

```bash
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method GetFsmId
./example/bin/aarch64/sr_3588_dds_echo --on-robot
```

电脑上：

```bash
./example/bin/x86_64/sr_3588_rpc_call --ip 192.168.5.2 --method GetFsmId
```

`--on-robot` 让 RPC 连本机 `127.0.0.1`。电脑上不带这个参数，并按实际地址加 `--ip`。SR_thro 把程序名里的 `sr_3588` 换成 `sr_thro`。

某一架构的 `example/bin/` 里还没有文件时，说明那一套还没放进来。先用已经放好的架构。

要自己重编示例时，在本目录执行：

```bash
cmake -S . -B build
cmake --build build -j
```

重编的程序在 `build/bin/`。交叉编译时指定目标架构的编译器，例如 `-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++`，CMake 会改用 `lib/aarch64` 和 `thirdparty/lib/aarch64`。系统的 glibc 或 libstdc++ 低于上面的版本时，配置会提示链不上 `thirdparty/lib/<架构>` 里的 `libfastdds.so`，并跳过 DDS 示例。`sr_3588_rpc_call` 仍然会编出来。

### 3. 查询状态机编号

```bash
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method GetFsmId
```

机器人不在默认地址、程序跑在电脑上时：

```bash
./example/bin/x86_64/sr_3588_rpc_call --ip 192.168.5.2 --method GetFsmId
```

成功时打印机器人返回的 JSON。`GetFsmId` 的 `result` 是当前状态机编号，`0` 也是合法编号。

连不上时会看到 `RPC 失败`，后面是错误名和系统错误。先核对 IP、端口 `51234`，以及本机到该地址的路由。默认超时 3000 毫秒，可以加 `--timeout-ms`。

要用原始 JSON 调其他方法时，用 `sr_3588_rpc_call`：

```bash
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method GetFsmId
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method SetFsmId --params '{"fsm_id":1}'
```

## 目录

| 路径 | 内容 |
|---|---|
| `include/dobot/sdk/` | DDS 会话、JSON-RPC 传输 |
| `include/dobot/robot/sr_3588/` | SR_3588 的话题名、RPC 方法和默认 IP |
| `include/dobot/robot/sr_thro/` | SR_thro 的话题名、RPC 方法和默认 IP |
| `include/dobot/idl/` | DDS 消息头文件 |
| `include/nlohmann/` | JSON 头文件 |
| `lib/aarch64/`、`lib/x86_64/` | `libdobot_sdk.a`、`libdobot_log.so` |
| `thirdparty/lib/aarch64/`、`thirdparty/lib/x86_64/` | `libfastdds.so`、`libfastcdr.so` |
| `example/bin/aarch64/`、`example/bin/x86_64/` | 对应架构已经编好的示例 |
| `example/src/sr_3588/`、`example/src/sr_thro/` | 各型号的示例源码，要改示例时再编 |
| `thirdparty/include/` | Fast DDS、Fast CDR、日志的头文件 |

自己重编的示例落在 `build/bin/`，从 `lib/<架构>` 找 `libdobot_log.so`，从 `thirdparty/lib/<架构>` 找 Fast DDS。DDS 的 domain 和网卡用 `SessionConfig` 的 `domain_id`、`network_interface`，或示例的 `--domain`、`--nic`。

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

这样会把示例也编进去。不需要示例时加上 `-DBUILD_EXAMPLES=OFF`。

当前系统能链上 Fast DDS 时，`dobot_sdk` 会带上 `fastdds` 和 `fastcdr`。链不上时，只调用 `RpcClient` 的程序仍然可以编译。
