// SR_thro 的上肢摆臂示例。持续写 rt/upper/cmd，直到 Ctrl+C。
// 先读当前上肢角度，腰、头和其余手臂关节保持该角度，只让双臂第一个关节来回摆。
#include "dobot/robot/sr_thro/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "upper_cmd.hpp"
#include "upper_cmdPubSubTypes.hpp"
#include "upper_state.hpp"
#include "upper_statePubSubTypes.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

namespace dds_ = dobot::msg::dds_;

constexpr int kUpperJoints = 18;
constexpr int kLeftArmJoint = 3;    // 左臂第 0 关节。上肢顺序：0–2 腰，3–9 左臂，10–16 右臂，17 头
constexpr int kRightArmJoint = 10;  // 右臂第 0 关节
constexpr uint8_t kMitMode = 11;
constexpr float kDefaultKp = 10.0f;
constexpr float kDefaultKd = 1.0f;
constexpr double kDefaultAmplitude = 0.20;
constexpr double kDefaultPeriod = 4.0;
constexpr double kDefaultHz = 100.0;
constexpr double kMaxAmplitude = 0.35;
constexpr double kMaxKp = 40.0;
constexpr double kMaxKd = 5.0;
constexpr double kPi = 3.14159265358979323846;

volatile std::sig_atomic_t g_stop = 0;

void OnSignal(int) {
    if (g_stop != 0) {
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
        std::raise(SIGINT);
        return;
    }
    g_stop = 1;
}

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0 << " --move [--on-robot] [--domain 0] [--nic 网卡名] [--amplitude 0.2] [--period 4] [--hz 100] [--kp 10] [--kd 1]\n"
              << "向 rt/upper/cmd 持续发送上肢命令，让双臂第一个关节绕当前角度来回摆。\n"
              << "腰、头和其余关节保持启动时读到的角度，不会收到 0。\n"
              << "amplitude 单位 rad，最大 " << kMaxAmplitude << "；period 单位秒，范围 2 到 30；hz 范围 20 到 200。\n"
              << "kp 最大 " << kMaxKp << "，kd 最大 " << kMaxKd << "。mode 固定为 " << static_cast<unsigned>(kMitMode) << "。\n"
              << "一直运行，Ctrl+C 后先回到起始角度再退出。再按一次 Ctrl+C 会立刻结束。\n"
              << "这是真实关节命令。机器人需要已经处于会执行 rt/upper/cmd 的使能状态。\n"
              << "DDS domain 默认 0。网卡默认全部，多网卡时加 --nic。\n";
}

struct Options {
    dobot::sdk::SessionConfig config;
    bool move = false;
    float amplitude = static_cast<float>(kDefaultAmplitude);
    float period = static_cast<float>(kDefaultPeriod);
    double hz = kDefaultHz;
    float kp = kDefaultKp;
    float kd = kDefaultKd;
};

bool ParseDouble(const char* text, const char* name, double* value) {
    try {
        *value = std::stod(text);
    } catch (const std::exception&) {
        std::cerr << "参数不是数字: " << name << "\n";
        return false;
    }
    return true;
}

dds_::UpperCmd_ MakeCmd(const std::array<float, kUpperJoints>& q0, float left_delta, float right_delta, float kp, float kd) {
    dds_::UpperCmd_ cmd;
    for (int i = 0; i < kUpperJoints; ++i) {
        float target = q0[static_cast<size_t>(i)];
        if (i == kLeftArmJoint) {
            target += left_delta;
        } else if (i == kRightArmJoint) {
            target += right_delta;
        }
        auto& motor = cmd.motor_cmd()[i];
        motor.mode(kMitMode);
        motor.q(target);
        motor.dq(0.0f);
        motor.tau(0.0f);
        motor.kp(kp);
        motor.kd(kd);
    }
    return cmd;
}

bool ReadPose(const dobot::sdk::Subscriber<dds_::UpperState_>& upper, std::array<float, kUpperJoints>* q0) {
    const auto sample = upper.Latest();
    if (!sample || static_cast<int>(sample->motor_state().size()) < kUpperJoints) {
        return false;
    }
    for (int i = 0; i < kUpperJoints; ++i) {
        (*q0)[static_cast<size_t>(i)] = sample->motor_state()[i].q();
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "缺少参数: " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            Usage(argv[0]);
            return 0;
        } else if (arg == "--move") {
            options.move = true;
        } else if (arg == "--on-robot") {
            options.config.where = dobot::sdk::RunWhere::kRobot;
        } else if (arg == "--domain") {
            options.config.domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            options.config.network_interface = need("--nic");
        } else if (arg == "--amplitude" || arg == "--period" || arg == "--hz" || arg == "--kp" || arg == "--kd") {
            double value = 0.0;
            if (!ParseDouble(need(arg.c_str()), arg.c_str(), &value)) {
                return 2;
            }
            if (arg == "--amplitude") {
                options.amplitude = static_cast<float>(value);
            } else if (arg == "--period") {
                options.period = static_cast<float>(value);
            } else if (arg == "--hz") {
                options.hz = value;
            } else if (arg == "--kp") {
                options.kp = static_cast<float>(value);
            } else {
                options.kd = static_cast<float>(value);
            }
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }
    if (!options.move) {
        Usage(argv[0]);
        return 2;
    }
    if (!(options.amplitude > 0.0f && options.amplitude <= static_cast<float>(kMaxAmplitude))) {
        std::cerr << "amplitude 要在 (0, " << kMaxAmplitude << "] rad\n";
        return 2;
    }
    if (!(options.period >= 2.0f && options.period <= 30.0f)) {
        std::cerr << "period 要在 [2, 30] 秒\n";
        return 2;
    }
    if (!(options.hz >= 20.0 && options.hz <= 200.0)) {
        std::cerr << "hz 要在 [20, 200]\n";
        return 2;
    }
    if (!(options.kp > 0.0f && options.kp <= static_cast<float>(kMaxKp)) || !(options.kd >= 0.0f && options.kd <= static_cast<float>(kMaxKd))) {
        std::cerr << "kp 要在 (0, " << kMaxKp << "]，kd 要在 [0, " << kMaxKd << "]\n";
        return 2;
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thro::Profile(), options.config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto upper = session.Subscribe<dds_::UpperState_PubSubType>(dobot::robot::sr_thro::kTopicUpperState);
    auto writer = session.Publish<dds_::UpperCmd_PubSubType>(dobot::robot::sr_thro::kTopicUpperCmd);
    if (!upper.valid() || !writer.valid()) {
        std::cerr << "创建订阅或发布失败\n";
        return 1;
    }
    if (!upper.WaitForData(std::chrono::seconds(5))) {
        std::cerr << "5 秒内没有收到 rt/upper/state，不发送命令\n";
        return 1;
    }

    std::cout << "等待 rt/upper/cmd 匹配。Ctrl+C 结束。\n" << std::flush;
    while (g_stop == 0 && writer.Matched() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (g_stop != 0) {
        std::cout << "匹配前已停止\n";
        return 0;
    }

    std::array<float, kUpperJoints> q0{};
    if (!ReadPose(upper, &q0)) {
        std::cerr << "上肢状态关节数不足 " << kUpperJoints << "，不发送命令\n";
        return 1;
    }

    std::cout << "开始摆臂。左臂下标 " << kLeftArmJoint << " 与右臂下标 " << kRightArmJoint << " 反向摆动 ±" << options.amplitude
              << " rad，周期 " << options.period << " s，" << options.hz << " Hz，kp=" << options.kp << " kd=" << options.kd << "。\n"
              << "其余上肢关节保持当前角度。Ctrl+C 后回到起始角度。\n"
              << std::flush;

    const auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(1.0 / options.hz));
    const auto motion_start = std::chrono::steady_clock::now();
    auto next_tick = motion_start;
    auto report_at = motion_start + std::chrono::seconds(1);
    uint64_t write_ok = 0;
    uint64_t write_fail = 0;
    uint64_t window_ok = 0;
    uint64_t window_fail = 0;
    float left_delta = 0.0f;
    float right_delta = 0.0f;

    while (g_stop == 0) {
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - motion_start).count();
        const double phase = std::sin(2.0 * kPi * elapsed / static_cast<double>(options.period));
        left_delta = options.amplitude * static_cast<float>(phase);
        right_delta = -left_delta;
        if (writer.Write(MakeCmd(q0, left_delta, right_delta, options.kp, options.kd))) {
            ++write_ok;
            ++window_ok;
        } else {
            ++write_fail;
            ++window_fail;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= report_at) {
            std::cout << "t=" << elapsed << "s left=" << (q0[kLeftArmJoint] + left_delta) << " right=" << (q0[kRightArmJoint] + right_delta)
                      << " matched=" << writer.Matched() << " write_ok=" << window_ok << " write_fail=" << window_fail << "\n"
                      << std::flush;
            window_ok = 0;
            window_fail = 0;
            report_at = now + std::chrono::seconds(1);
        }

        next_tick += dt;
        if (std::chrono::steady_clock::now() > next_tick) {
            next_tick = std::chrono::steady_clock::now();
        }
        std::this_thread::sleep_until(next_tick);
    }

    std::cout << "正在回到起始角度\n" << std::flush;
    next_tick = std::chrono::steady_clock::now();
    constexpr double kReturnSec = 1.0;
    const int return_steps = std::max(1, static_cast<int>(options.hz * kReturnSec));
    const float left_from = left_delta;
    const float right_from = right_delta;
    for (int step = 1; step <= return_steps; ++step) {
        const float scale = 1.0f - static_cast<float>(step) / static_cast<float>(return_steps);
        (void)writer.Write(MakeCmd(q0, left_from * scale, right_from * scale, options.kp, options.kd));
        next_tick += dt;
        std::this_thread::sleep_until(next_tick);
    }

    std::cout << "已停止 write_ok=" << write_ok << " write_fail=" << write_fail << "\n";
    return 0;
}
