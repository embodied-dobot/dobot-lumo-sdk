// SR_3588 的上肢摆臂示例。持续写 rt/upper/cmd，直到 Ctrl+C。
// 先读当前上肢角度。双臂第二个关节先向外让出 10°，再以这个角度为内侧起点来回摆。
// Ctrl+C 后回到启动时的贴身角度。
#include "dobot/robot/sr_3588/topics.hpp"
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
constexpr int kLeftArmJoint = 4;    // 左臂第 1 关节，向外为正。上肢顺序：0–2 腰，3–9 左臂，10–16 右臂，17 头
constexpr int kRightArmJoint = 11;  // 右臂第 1 关节，向外为负
constexpr float kLeftLateralMin = -0.43f;
constexpr float kLeftLateralMax = 3.14f;
constexpr float kRightLateralMin = -3.14f;
constexpr float kRightLateralMax = 0.43f;
constexpr uint8_t kMitMode = 11;
constexpr float kDefaultKp = 10.0f;
constexpr float kDefaultKd = 1.0f;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDefaultAmplitude = kPi / 2.0;
constexpr double kDefaultPeriod = 2.0;
constexpr double kDefaultHz = 1000.0;
constexpr double kMaxHz = 1000.0;
constexpr double kMaxAmplitude = kPi / 2.0;
constexpr double kMaxKp = 40.0;
constexpr double kMaxKd = 5.0;
constexpr double kClearanceRad = 10.0 * kPi / 180.0;
constexpr double kApproachSec = 1.0;

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
    std::cerr << "用法: " << argv0 << " --move [--on-robot] [--domain 0] [--nic 网卡名] [--amplitude 1.57] [--period 2] [--hz 1000] [--kp 10] [--kd 1]\n"
              << "向 rt/upper/cmd 持续发送上肢命令。双臂第二个关节先向外让出 10°，再以这个角度为内侧起点向外抬起。\n"
              << "往复只回到这 10°，不会摆回贴身位置。腰、头和其余关节保持启动时读到的角度，不会收到 0。\n"
              << "amplitude 是在这 10° 之外再向外的幅度，单位 rad，最大 " << kMaxAmplitude << "；period 是向外再回到这 10° 的秒数，范围 2 到 30；hz 范围 20 到 " << kMaxHz << "。\n"
              << "kp 最大 " << kMaxKp << "，kd 最大 " << kMaxKd << "。mode 固定为 " << static_cast<unsigned>(kMitMode) << "。\n"
              << "一直运行，Ctrl+C 后先回到启动时的贴身角度再退出。再按一次 Ctrl+C 会立刻结束。\n"
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

struct JointTarget {
    float q = 0.0f;
    float dq = 0.0f;
};

JointTarget LimitJoint(float q, float delta, float dq, float q_min, float q_max) {
    JointTarget out;
    out.q = q + delta;
    out.dq = dq;
    if (out.q > q_max) {
        out.q = q_max;
        if (out.dq > 0.0f) {
            out.dq = 0.0f;
        }
    } else if (out.q < q_min) {
        out.q = q_min;
        if (out.dq < 0.0f) {
            out.dq = 0.0f;
        }
    }
    return out;
}

dds_::UpperCmd_ MakeCmd(const std::array<float, kUpperJoints>& q0, const JointTarget& left, const JointTarget& right, float kp, float kd) {
    dds_::UpperCmd_ cmd;
    for (int i = 0; i < kUpperJoints; ++i) {
        float target = q0[static_cast<size_t>(i)];
        float vel = 0.0f;
        if (i == kLeftArmJoint) {
            target = left.q;
            vel = left.dq;
        } else if (i == kRightArmJoint) {
            target = right.q;
            vel = right.dq;
        }
        auto& motor = cmd.motor_cmd()[i];
        motor.mode(kMitMode);
        motor.q(target);
        motor.dq(vel);
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
    if (!(options.amplitude > 0.0f && static_cast<double>(options.amplitude) <= kMaxAmplitude + 1.0e-4)) {
        std::cerr << "amplitude 要在 (0, " << kMaxAmplitude << "] rad\n";
        return 2;
    }
    if (!(options.period >= 2.0f && options.period <= 30.0f)) {
        std::cerr << "period 要在 [2, 30] 秒\n";
        return 2;
    }
    if (!(options.hz >= 20.0 && options.hz <= kMaxHz)) {
        std::cerr << "hz 要在 [20, " << kMaxHz << "]\n";
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
    if (!session.Init(dobot::robot::sr_3588::Profile(), options.config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto upper = session.Subscribe<dds_::UpperState_PubSubType>(dobot::robot::sr_3588::kTopicUpperState);
    auto writer = session.Publish<dds_::UpperCmd_PubSubType>(dobot::robot::sr_3588::kTopicUpperCmd);
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

    std::cout << "开始摆臂。左臂下标 " << kLeftArmJoint << " 与右臂下标 " << kRightArmJoint << " 先向外让出 " << kClearanceRad
              << " rad，再向外抬起 " << options.amplitude << " rad，往复回到这 10°，来回 " << options.period << " s，" << options.hz
              << " Hz，kp=" << options.kp << " kd=" << options.kd << "。\n"
              << "其余上肢关节保持当前角度。Ctrl+C 后回到贴身角度。\n"
              << std::flush;

    const auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(1.0 / options.hz));
    auto next_tick = std::chrono::steady_clock::now();
    auto report_at = next_tick + std::chrono::seconds(1);
    uint64_t write_ok = 0;
    uint64_t write_fail = 0;
    uint64_t window_ok = 0;
    uint64_t window_fail = 0;
    float left_delta = 0.0f;
    float right_delta = 0.0f;
    JointTarget left;
    JointTarget right;

    const int approach_steps = std::max(1, static_cast<int>(options.hz * kApproachSec));
    for (int step = 1; step <= approach_steps && g_stop == 0; ++step) {
        const double tau = static_cast<double>(step) / static_cast<double>(approach_steps);
        const double outward = kClearanceRad * 0.5 * (1.0 - std::cos(kPi * tau));
        const double outward_vel = kClearanceRad * 0.5 * std::sin(kPi * tau) * (kPi / kApproachSec);
        left_delta = static_cast<float>(outward);
        right_delta = -left_delta;
        left = LimitJoint(q0[static_cast<size_t>(kLeftArmJoint)], left_delta, static_cast<float>(outward_vel), kLeftLateralMin, kLeftLateralMax);
        right = LimitJoint(q0[static_cast<size_t>(kRightArmJoint)], right_delta, static_cast<float>(-outward_vel), kRightLateralMin, kRightLateralMax);
        if (writer.Write(MakeCmd(q0, left, right, options.kp, options.kd))) {
            ++write_ok;
        } else {
            ++write_fail;
        }
        next_tick += dt;
        if (std::chrono::steady_clock::now() > next_tick) {
            next_tick = std::chrono::steady_clock::now();
        }
        std::this_thread::sleep_until(next_tick);
    }

    const auto swing_start = std::chrono::steady_clock::now();
    while (g_stop == 0) {
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - swing_start).count();
        const double period = static_cast<double>(options.period);
        const double angle = 2.0 * kPi * elapsed / period;
        const double swing = static_cast<double>(options.amplitude) * 0.5 * (1.0 - std::cos(angle));
        const double outward = kClearanceRad + swing;
        const double outward_vel = static_cast<double>(options.amplitude) * (kPi / period) * std::sin(angle);
        left_delta = static_cast<float>(outward);
        right_delta = -left_delta;
        left = LimitJoint(q0[static_cast<size_t>(kLeftArmJoint)], left_delta, static_cast<float>(outward_vel), kLeftLateralMin, kLeftLateralMax);
        right = LimitJoint(q0[static_cast<size_t>(kRightArmJoint)], right_delta, static_cast<float>(-outward_vel), kRightLateralMin, kRightLateralMax);
        if (writer.Write(MakeCmd(q0, left, right, options.kp, options.kd))) {
            ++write_ok;
            ++window_ok;
        } else {
            ++write_fail;
            ++window_fail;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= report_at) {
            std::cout << "t=" << elapsed << "s left=" << left.q << " right=" << right.q << " matched=" << writer.Matched() << " write_ok=" << window_ok
                      << " write_fail=" << window_fail << "\n"
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

    std::cout << "正在回到贴身角度\n" << std::flush;
    next_tick = std::chrono::steady_clock::now();
    constexpr double kReturnSec = 1.0;
    const int return_steps = std::max(1, static_cast<int>(options.hz * kReturnSec));
    const float left_from = left_delta;
    const float right_from = right_delta;
    for (int step = 1; step <= return_steps; ++step) {
        const float scale = 1.0f - static_cast<float>(step) / static_cast<float>(return_steps);
        const float vel = step == return_steps ? 0.0f : -1.0f / static_cast<float>(kReturnSec);
        left = LimitJoint(q0[static_cast<size_t>(kLeftArmJoint)], left_from * scale, left_from * vel, kLeftLateralMin, kLeftLateralMax);
        right = LimitJoint(q0[static_cast<size_t>(kRightArmJoint)], right_from * scale, right_from * vel, kRightLateralMin, kRightLateralMax);
        (void)writer.Write(MakeCmd(q0, left, right, options.kp, options.kd));
        next_tick += dt;
        std::this_thread::sleep_until(next_tick);
    }

    std::cout << "已停止 write_ok=" << write_ok << " write_fail=" << write_fail << "\n";
    return 0;
}
