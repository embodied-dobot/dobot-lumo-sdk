#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <dobot_fastdds.hpp>

#include "dobot/motion/motion.hpp"
#include "dobot/sdk/channel.hpp"
#include "dobot/sdk/motion_rpc.hpp"
#include "dobot/sdk/robot_profile.hpp"

namespace eprosima::fastdds::dds {
class DataReader;
}

namespace dobot::sdk {

// Where the application runs. Both sides use the same Session; only the default addresses differ.
enum class RunWhere {
    kComputer,  // User PC, connected to the robot by Ethernet
    kRobot,     // Program runs on the robot itself
};

// robot_ip is used by RPC only. When empty, a PC uses the model's default IP and the robot itself uses 127.0.0.1.
// domain_id defaults to 0. When network_interface is empty, all network interfaces take part in DDS discovery.
// Topic QoS comes from the config compiled into the library, looked up as "<topic>.reader" and "<topic>.writer".
// async_subscribe defaults to false: subscribe callbacks run on the Fast DDS listener thread, right after take.
// When true, callbacks run on a worker of an ordered queue instead.
struct SessionConfig {
    RunWhere where = RunWhere::kComputer;
    std::string robot_ip;
    int rpc_port = 0;
    uint32_t domain_id = 0;
    std::string network_interface;
    bool async_subscribe = false;
};

// Connection to one robot. A Session owns the RPC and DDS on that connection.
// To connect to two robots from one process, create two Sessions. Do not make it a process-wide singleton.
//
// Application entry points:
//   session.Init(robot profile, config)
//   session.motion().SetRobotMode(...)
//   session.rpc().GetFsmId()   // thin RPC, for debugging or methods not yet wrapped
//   auto state = session.Subscribe<message PubSubType>(topic name)
//   state.Latest()
//   session.Publish<message PubSubType>(topic name).Write(cmd)
//
// Call Init and Shutdown from an application thread, not from a subscribe callback.
class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Open RPC and DDS. Returns false if already initialized; call Shutdown before Init again.
    bool Init(const RobotProfile& profile, const SessionConfig& config, std::string* error);
    // Wait for running callbacks to finish, then release this session's DDS and RPC. Does not require the robot to stop.
    void Shutdown();

    bool open() const { return open_.load(); }
    const RobotProfile& profile() const { return profile_; }

    MotionRpc& rpc() { return motion_; }
    const MotionRpc& rpc() const { return motion_; }

    // Semantic locomotion API: modes, preset actions and dances. Uses MotionRpc internally.
    dobot::motion::Motion motion() const { return dobot::motion::Motion(motion_); }

    // For new RPCs that exist on one model only. Add methods shared by both models to MotionRpc instead of bypassing it here.
    RpcResult Call(const std::string& method, const nlohmann::json& params, int timeout_ms = 3000) const;

    // The template parameter is the PubSubType from generated code, e.g. dobot::msg::dds_::LowerState_PubSubType.
    // QoS uses the built-in "<topic>.reader" config. One PubSubType can subscribe to only one topic.
    template <typename PubSubType>
    Subscriber<typename PubSubType::type> Subscribe(const std::string& topic);

    // QoS uses the built-in "<topic>.writer" config. Only one publisher is created per topic; call Write repeatedly in the control loop.
    template <typename PubSubType>
    Publisher<typename PubSubType::type> Publish(const std::string& topic);

private:
    struct ReaderEndpoint {
        std::mutex mu;
        eprosima::fastdds::dds::DataReader* reader = nullptr;
    };
    struct SubEntry {
        std::string topic;
        std::shared_ptr<void> state;
    };
    struct PubEntry {
        std::type_index type;
        std::shared_ptr<void> state;
    };

    RobotProfile profile_{};
    SessionConfig config_{};
    std::atomic<bool> open_{false};
    bool inited_ = false;

    std::mutex mu_;
    std::shared_ptr<dobot::dds::DdsClientFramework> framework_;
    std::unique_ptr<RpcClient> rpc_client_;
    MotionRpc motion_;
    std::unordered_map<std::type_index, SubEntry> subscriptions_;
    std::unordered_map<std::string, PubEntry> publications_;
    std::vector<std::function<void()>> close_endpoints_;
};

}  // namespace dobot::sdk

#include "dobot/sdk/session.inl"
