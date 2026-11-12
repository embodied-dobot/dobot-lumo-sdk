// SR_thor: Switch to humanoid walking, walk forward for a while, then backward, and finally stop.
// The robot moves only with --run. Ctrl+C sends zero velocity once more.
#include "dobot/motion/motion.hpp"
#include "dobot/robot/sr_thor/topics.hpp"
#include "dobot/sdk/rpc_client.hpp"
#include "dobot/sdk/session.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t g_stop = 0;

void OnSignal(int) { g_stop = 1; }

void Usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " --run [--vx 0.2] [--seconds 2] [--laps 1] [--timeout-ms 3000] [--ip "
              << dobot::robot::sr_thor::kDefaultRobotIp << "] [--on-robot] [--domain 0] [--nic <nic>]\n"
              << "Switches to humanoid walking, walks forward at --vx for --seconds seconds, then backward at the same speed, repeats --laps times, and finally stops.\n"
              << "Without --run the robot will not move. Velocity is in m/s.\n";
}

bool WaitSeconds(float seconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<float>(seconds);
    while (g_stop == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return g_stop == 0;
}

int Fail(const char* what, int32_t code) {
    std::cerr << what << " failed " << dobot::sdk::RpcErrorName(static_cast<dobot::sdk::RpcError>(code)) << "\n";
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    bool run = false;
    bool ip_set = false;
    bool on_robot = false;
    uint32_t domain_id = 0;
    std::string network_interface;
    std::string ip;
    float vx = 0.2f;
    float seconds = 2.0f;
    int laps = 1;
    int timeout_ms = 3000;

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
        } else if (arg == "--run") {
            run = true;
        } else if (arg == "--vx") {
            vx = std::stof(need("--vx"));
        } else if (arg == "--seconds") {
            seconds = std::stof(need("--seconds"));
        } else if (arg == "--laps") {
            laps = std::stoi(need("--laps"));
        } else if (arg == "--timeout-ms") {
            timeout_ms = std::stoi(need("--timeout-ms"));
        } else if (arg == "--ip") {
            ip = need("--ip");
            ip_set = true;
        } else if (arg == "--on-robot") {
            on_robot = true;
        } else if (arg == "--domain") {
            domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            network_interface = need("--nic");
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }

    if (!run) {
        std::cerr << "--run not given; the robot will not move.\n";
        Usage(argv[0]);
        return 2;
    }
    if (vx <= 0.0f || seconds <= 0.0f || laps < 1) {
        std::cerr << "--vx and --seconds must be greater than 0, and --laps must be at least 1.\n";
        return 2;
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    dobot::sdk::SessionConfig config;
    config.where = on_robot ? dobot::sdk::RunWhere::kRobot : dobot::sdk::RunWhere::kComputer;
    if (ip_set) {
        config.robot_ip = ip;
    }
    config.domain_id = domain_id;
    config.network_interface = network_interface;

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thor::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    const dobot::motion::Motion motion = session.motion();
    int32_t code = motion.SetRobotMode(dobot::motion::RobotMode::kWalking, timeout_ms);
    if (code != 0) {
        return Fail("SetRobotMode(walk)", code);
    }
    std::cout << "Switched to humanoid walking. Forward " << vx << " m/s, " << seconds << " s per leg, " << laps << " round trip(s).\n";

    bool moving = false;
    for (int lap = 1; lap <= laps && g_stop == 0; ++lap) {
        std::cout << "Round trip " << lap << ": forward\n";
        code = motion.SetVelocity(vx, 0.0f, 0.0f, seconds, timeout_ms);
        if (code != 0) {
            return Fail("SetVelocity forward", code);
        }
        moving = true;
        if (!WaitSeconds(seconds)) {
            break;
        }
        std::cout << "Round trip " << lap << ": backward\n";
        code = motion.SetVelocity(-vx, 0.0f, 0.0f, seconds, timeout_ms);
        if (code != 0) {
            motion.SetVelocity(0.0f, 0.0f, 0.0f, 1.0f, timeout_ms);
            return Fail("SetVelocity backward", code);
        }
        if (!WaitSeconds(seconds)) {
            break;
        }
    }

    if (moving || g_stop != 0) {
        code = motion.SetVelocity(0.0f, 0.0f, 0.0f, 1.0f, timeout_ms);
        if (code != 0) {
            return Fail("SetVelocity stop", code);
        }
        std::cout << "Stopped\n";
    }
    return 0;
}
