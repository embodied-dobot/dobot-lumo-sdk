# dobot-sdk

**English** | [简体中文](./README.zh-CN.md)

C++ SDK for controlling and monitoring Dobot humanoid robots (SR_3588 and SR_thor) over the network, or from the robot itself, via JSON-RPC and Fast DDS.

This directory contains the public headers, prebuilt libraries, and prebuilt examples. It does not contain the SDK implementation.

> **Status:** Early release (version 0.2.0). The API may change between major versions; adding functions or fields is a minor-version change, and deprecated functions are kept for one version before removal.

## Supported robots

Both models share the same RPC methods and DDS topics. Each has its own header set.

| Model | Namespace | Default IP | RPC port | DDS domain |
|---|---|---|---|---|
| SR_3588 | `dobot::robot::sr_3588` | `192.168.5.2` | `51234` | `0` |
| SR_thor | `dobot::robot::sr_thor` | `172.16.20.2` | `51234` | `0` |

## Quick start

### Prerequisites

- Linux on x86_64 or aarch64, with GLIBC 2.34 and GLIBCXX 3.4.30 or later (Ubuntu 22.04 qualifies)
- C++17 compiler and CMake 3.16 or later (only needed to rebuild the examples or use the SDK in your project)
- Network access to the robot's IP address

The prebuilt libraries are in `lib/<arch>` and `thirdparty/lib/<arch>`. CMake selects the directory that matches the target architecture of your compiler.

### Run a prebuilt example

The steps below use SR_3588. For SR_thor, replace `sr_3588` with `sr_thor` in the program name and use IP `172.16.20.2`.

1. Check that the robot is reachable:

   ```bash
   ping -c 3 192.168.5.2
   ```

   If the robot is not at the default address, pass its actual address with `--ip` in the commands below.

2. Query the current state machine (FSM) ID. Use `example/bin/x86_64` on a PC and `example/bin/aarch64` on the robot:

   ```bash
   # On a PC
   ./example/bin/x86_64/sr_3588_motion_demo --ip 192.168.5.2

   # On the robot itself
   ./example/bin/aarch64/sr_3588_motion_demo --on-robot
   ```

Expected output (the program prints the current state machine ID and a short name; `0` is a valid ID):

```text
Current FSM 300 (standing)
```

If the connection fails, the program prints `GetFsmId failed` followed by the error name. Check the IP, port `51234`, and the route from your machine to the robot. The default timeout is 3000 ms; change it with `--timeout-ms`.

Common options:

| Option | Meaning |
|---|---|
| `--on-robot` | Connect RPC to `127.0.0.1` (when running on the robot) |
| `--ip <addr>` | Robot address (when running on a PC) |
| `--nic <name>` | Network interface for DDS discovery; use it when the PC has several interfaces |
| `--domain <id>` | DDS domain, if it is not `0` |
| `--timeout-ms <ms>` | RPC timeout |

### Other examples

Binaries are named `sr_3588_<name>` / `sr_thor_<name>`; sources are in `example/src/<model>/`.

| Example | What it does |
|---|---|
| `motion_demo` | Query the FSM ID. `--mode stand` switches to Mini stand, `--action` runs a preset action, `--dance` runs a dance |
| `walk_demo` | Walk back and forth (requires `--run`) |
| `rpc_call` | Send a raw JSON-RPC request (requires `--method`); for skills, action files, and methods without a wrapper |
| `dds_echo` | Subscribe to lower- and upper-body state |
| `dds_publish_cmd` | Publish one lower-body command frame (requires `--send`) |
| `dds_event` | Subscribe to the e-stop topic and run a callback |
| `tts_demo` | Send text to the voice service and wait for the playback result |
| `action_watch` | Print when a skill starts and ends (`--all` prints every frame) |
| `state_gap`, `state_gap_pro`, `upper_state_rate`, `upper_arm_swing` | State-rate and latency checks, and an arm-swing demo |

For example, to call a raw RPC method on the robot:

```bash
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method ReloadArmSkillsData
./example/bin/aarch64/sr_3588_rpc_call --on-robot --method ExecuteAction --params '{"file_path":"skill.csv"}'
```

> [!WARNING]
> `walk_demo`, `dds_publish_cmd`, and the action examples move the physical robot. Make sure the area around the robot is clear and an e-stop is within reach.

### Rebuild the examples

```bash
cmake -S . -B build
cmake --build build -j
```

Binaries are written to `build/bin/`. To cross-compile, pass the target compiler, for example `-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++`; CMake then uses `lib/aarch64` and `thirdparty/lib/aarch64`.

If the system glibc or libstdc++ is older than required, configuration reports that `libfastdds.so` cannot be linked and skips the DDS examples. `motion_demo`, `walk_demo`, and `rpc_call` are still built.

## Use the SDK in your project

Install, then use `find_package`:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/dobot_sdk
cmake --build build
cmake --install build
```

```cmake
find_package(dobot_sdk REQUIRED)
target_link_libraries(my_app PRIVATE dobot_sdk)
```

If the install prefix is not on CMake's search path, configure your project with `-DCMAKE_PREFIX_PATH=/opt/dobot_sdk`.

Alternatively, add this directory to your project without installing:

```cmake
add_subdirectory(dobot-sdk)
target_link_libraries(my_app PRIVATE dobot_sdk)
```

This also builds the examples; pass `-DBUILD_EXAMPLES=OFF` to skip them. When Fast DDS can be linked on the current system, `dobot_sdk` also links `fastdds` and `fastcdr`.

Applications connect through `dobot::sdk::Session`. DDS domain and network interface are set with `SessionConfig::domain_id` and `SessionConfig::network_interface`.

## Directory layout

| Path | Contents |
|---|---|
| `include/dobot/sdk/` | DDS session and JSON-RPC transport |
| `include/dobot/robot/sr_3588/`, `include/dobot/robot/sr_thor/` | Topic names, RPC methods, and default IP per model |
| `include/dobot/idl/` | DDS message headers |
| `include/nlohmann/` | JSON headers |
| `lib/<arch>/` | `libdobot_sdk.a`, `libdobot_log.so` |
| `thirdparty/lib/<arch>/`, `thirdparty/include/` | Fast DDS and Fast CDR libraries and headers |
| `example/bin/<arch>/` | Prebuilt examples |
| `example/src/<model>/` | Example sources |

## Documentation

- [User guide](./docs/user-guide.md) — Network setup, subscribing and publishing, and the list of topics.
- [DDS topics](./docs/dds-topics.md) — Fields of each DDS topic.

## Support and contact

- **Bug reports and feature requests:** {{ISSUE_TRACKER_URL}}
- **Maintainers:** {{MAINTAINER_NAME_OR_EMAIL}}

When reporting a bug, include the SDK version (`GetSdkVersion()`), robot model, your OS and architecture, steps to reproduce, and expected versus actual behavior.

## Security

Do not report security vulnerabilities in public issues. Report them privately to {{SECURITY_CONTACT}}.

## License

{{LICENSE_NAME}}. See [LICENSE](./LICENSE).
