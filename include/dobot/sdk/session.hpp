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

#include "dobot/action/action.hpp"
#include "dobot/sdk/channel.hpp"
#include "dobot/sdk/motion_rpc.hpp"
#include "dobot/sdk/robot_profile.hpp"

namespace eprosima::fastdds::dds {
class DataReader;
}

namespace dobot::sdk {

// 二开程序放在哪。两边用同一个 Session，默认地址不同。
enum class RunWhere {
    kComputer,  // 用户电脑，网线连机器人
    kRobot,     // 程序跑在机器人本机
};

// robot_ip 只给 RPC。留空时，电脑用机型默认 IP，机器人本机用 127.0.0.1。
// domain_id 默认 0。network_interface 留空时，所有网卡都参与 DDS 发现。
// 话题 QoS 用编译进库的配置，按「话题名.reader」和「话题名.writer」取。
struct SessionConfig {
    RunWhere where = RunWhere::kComputer;
    std::string robot_ip;
    int rpc_port = 0;
    uint32_t domain_id = 0;
    std::string network_interface;
};

// 连上一台机器人。一个 Session 拥有这条连接上的 RPC 和 DDS。
// 一个进程要连两台，就建两个 Session。不要做成进程里唯一的全局对象。
//
// 应用代码从这里进：
//   session.Init(机型描述, 配置)
//   session.rpc().GetFsmId()
//   auto state = session.Subscribe<消息的 PubSubType>(话题名)
//   state.Latest()
//   session.Publish<消息的 PubSubType>(话题名).Write(cmd)
//
// Init 和 Shutdown 在业务线程调用，不要在订阅回调里调用。
class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // 打开 RPC 和 DDS。重复初始化返回 false，先 Shutdown 再 Init。
    bool Init(const RobotProfile& profile, const SessionConfig& config, std::string* error);
    // 等正在执行的回调结束，再释放本会话的 DDS 和 RPC。不要求机器人停机。
    void Shutdown();

    bool open() const { return open_.load(); }
    const RobotProfile& profile() const { return profile_; }

    MotionRpc& rpc() { return motion_; }
    const MotionRpc& rpc() const { return motion_; }

    // 动作业务。封装现有 ExecuteAction，不把 JSON 交给调用方。
    dobot::action::Action action() const { return dobot::action::Action(motion_); }

    // 给某一台独有的新 RPC 用。两台都有的方法加到 MotionRpc，不要从这里绕过。
    RpcResult Call(const std::string& method, const nlohmann::json& params, int timeout_ms = 3000) const;

    // 模板参数是生成代码里的 PubSubType，例如 dobot::msg::dds_::LowerState_PubSubType。
    // QoS 使用库内配置的「话题名.reader」。同一 PubSubType 只能订一个话题。
    template <typename PubSubType>
    Subscriber<typename PubSubType::type> Subscribe(const std::string& topic);

    // QoS 使用库内配置的「话题名.writer」。一条话题只建一个发布端，控制环里反复 Write。
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
