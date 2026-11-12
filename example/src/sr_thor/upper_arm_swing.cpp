// SR_thor upper-body arm swing example. Continuously writes rt/upper/cmd until Ctrl+C.
// First reads the current upper-body angles; waist, head and the other arm joints hold them, and only the first joint of each arm swings.
#include "dobot/robot/sr_thor/topics.hpp"
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
constexpr int kLeftArmJoint = 3;    // Left arm joint 0. Upper-body order: 0-2 waist, 3-9 left arm, 10-16 right arm, 17 head
constexpr int kRightArmJoint = 10;  // Right arm joint 0
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
    std::cerr << "Usage: " << argv0 << " --move [--on-robot] [--domain 0] [--nic <nic>] [--amplitude 0.2] [--period 4] [--hz 100] [--kp 10] [--kd 1]\n"
              << "Continuously sends upper-body commands to rt/upper/cmd so the first joint of each arm swings around its current angle.\n"
              << "Waist, head and other joints hold the angles read at startup and never receive 0.\n"
              << "amplitude in rad, max " << kMaxAmplitude << "; period in seconds, range 2 to 30; hz range 20 to 200.\n"
              << "kp max " << kMaxKp << ", kd max " << kMaxKd << ". mode is fixed at " << static_cast<unsigned>(kMitMode) << "。\n"
              << "Runs until Ctrl+C, then returns to the starting angles before exiting. Press Ctrl+C again to exit immediately.\n"
              << "These are real joint commands. The robot must already be in an enabled state that executes rt/upper/cmd.\n"
              << "DDS domain defaults to 0. All network interfaces are used by default; with several, add --nic.\n";
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
        std::cerr << "Argument is not a number: " << name << "\n";
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
                std::cerr << "Missing argument: " << name << "\n";
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
            std::cerr << "Unknown argument: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }
    if (!options.move) {
        Usage(argv[0]);
        return 2;
    }
    if (!(options.amplitude > 0.0f && options.amplitude <= static_cast<float>(kMaxAmplitude))) {
        std::cerr << "amplitude must be in (0, " << kMaxAmplitude << "] rad\n";
        return 2;
    }
    if (!(options.period >= 2.0f && options.period <= 30.0f)) {
        std::cerr << "period must be in [2, 30] seconds\n";
        return 2;
    }
    if (!(options.hz >= 20.0 && options.hz <= 200.0)) {
        std::cerr << "hz must be in [20, 200]\n";
        return 2;
    }
    if (!(options.kp > 0.0f && options.kp <= static_cast<float>(kMaxKp)) || !(options.kd >= 0.0f && options.kd <= static_cast<float>(kMaxKd))) {
        std::cerr << "kp must be in (0, " << kMaxKp << "], kd must be in [0, " << kMaxKd << "]\n";
        return 2;
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thor::Profile(), options.config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto upper = session.Subscribe<dds_::UpperState_PubSubType>(dobot::robot::sr_thor::kTopicUpperState);
    auto writer = session.Publish<dds_::UpperCmd_PubSubType>(dobot::robot::sr_thor::kTopicUpperCmd);
    if (!upper.valid() || !writer.valid()) {
        std::cerr << "Failed to create subscriber or publisher\n";
        return 1;
    }
    if (!upper.WaitForData(std::chrono::seconds(5))) {
        std::cerr << "No rt/upper/state received within 5 s; not sending commands\n";
        return 1;
    }

    std::cout << "Waiting for rt/upper/cmd to match. Press Ctrl+C to stop.\n" << std::flush;
    while (g_stop == 0 && writer.Matched() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (g_stop != 0) {
        std::cout << "Stopped before matching\n";
        return 0;
    }

    std::array<float, kUpperJoints> q0{};
    if (!ReadPose(upper, &q0)) {
        std::cerr << "Upper-body state has too few joints: need " << kUpperJoints << "; not sending commands\n";
        return 1;
    }

    std::cout << "Starting arm swing. Left arm index " << kLeftArmJoint << " and right arm index " << kRightArmJoint << " swing in opposite directions by +/-" << options.amplitude
              << " rad, period " << options.period << " s，" << options.hz << " Hz，kp=" << options.kp << " kd=" << options.kd << "。\n"
              << "Other upper-body joints hold their current angles. After Ctrl+C, returns to the starting angles.\n"
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

    std::cout << "Returning to the starting angles\n" << std::flush;
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

    std::cout << "Stopped. write_ok=" << write_ok << " write_fail=" << write_fail << "\n";
    return 0;
}
