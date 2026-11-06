// SR_3588 文字转语音示例。向 rt/voice/tts_request 发一块文本，
// 并订阅 rt/voice/output_event，直到播放结束或被拒绝。
#include "dobot/robot/sr_3588/topics.hpp"
#include "dobot/sdk/session.hpp"

#include "voice.hpp"
#include "voicePubSubTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace dds_ = dobot::msg::dds_;

namespace {

void Usage(const char* argv0) {
    std::cerr << "用法: " << argv0
              << " [--text 要播报的文字] [--source agent] [--seconds 15]"
              << " [--on-robot] [--domain 0] [--nic 网卡名]\n"
              << "向 rt/voice/tts_request 发一块文本，并订阅 rt/voice/output_event。\n"
              << "source 必须是 Voice 来源策略里允许 TTS 的名字，文档示例是 agent。\n"
              << "DDS domain 默认 0。网卡默认全部，多网卡时加 --nic。\n";
}

const char* EventName(dds_::OutputEventType event) {
    switch (event) {
        case dds_::OutputEventType::REQUEST_BUFFERED:
            return "REQUEST_BUFFERED";
        case dds_::OutputEventType::REQUEST_ACCEPTED:
            return "REQUEST_ACCEPTED";
        case dds_::OutputEventType::REQUEST_REJECTED:
            return "REQUEST_REJECTED";
        case dds_::OutputEventType::REQUEST_DROPPED:
            return "REQUEST_DROPPED";
        case dds_::OutputEventType::PLAYBACK_STARTED:
            return "PLAYBACK_STARTED";
        case dds_::OutputEventType::PLAYBACK_COMPLETED:
            return "PLAYBACK_COMPLETED";
        case dds_::OutputEventType::PLAYBACK_INTERRUPTED:
            return "PLAYBACK_INTERRUPTED";
        case dds_::OutputEventType::PLAYBACK_FAILED:
            return "PLAYBACK_FAILED";
    }
    return "UNKNOWN";
}

bool IsTerminal(dds_::OutputEventType event) {
    return event == dds_::OutputEventType::REQUEST_REJECTED ||
           event == dds_::OutputEventType::REQUEST_DROPPED ||
           event == dds_::OutputEventType::PLAYBACK_COMPLETED ||
           event == dds_::OutputEventType::PLAYBACK_INTERRUPTED ||
           event == dds_::OutputEventType::PLAYBACK_FAILED;
}

int64_t UnixMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

int main(int argc, char** argv) {
    dobot::sdk::SessionConfig config;
    std::string text = "当前状态正常。";
    std::string source = "agent";
    int seconds = 15;
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
        } else if (arg == "--on-robot") {
            config.where = dobot::sdk::RunWhere::kRobot;
        } else if (arg == "--domain") {
            config.domain_id = static_cast<uint32_t>(std::stoul(need("--domain")));
        } else if (arg == "--nic") {
            config.network_interface = need("--nic");
        } else if (arg == "--text") {
            text = need("--text");
        } else if (arg == "--source") {
            source = need("--source");
        } else if (arg == "--seconds") {
            seconds = std::stoi(need("--seconds"));
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_3588::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto events = session.Subscribe<dds_::VoiceOutputEvent_PubSubType>(
        dobot::robot::sr_3588::kTopicVoiceOutputEvent);
    auto writer = session.Publish<dds_::TTSRequest_PubSubType>(dobot::robot::sr_3588::kTopicVoiceTtsRequest);
    if (!events.valid() || !writer.valid()) {
        std::cerr << "创建订阅或发布失败\n";
        return 1;
    }

    const std::string speak_id = "speak-demo-" + std::to_string(UnixMs());
    std::atomic<int> terminal{-1};
    events.On([speak_id, &terminal](const dds_::VoiceOutputEvent_& msg) {
        if (msg.speak_id() != speak_id) {
            return;
        }
        const auto event = msg.event_type();
        std::cout << "output_event " << EventName(event) << " reason=" << static_cast<int>(msg.reason())
                  << "\n";
        if (IsTerminal(event)) {
            terminal.store(static_cast<int>(event));
        }
    });

    const auto match_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < match_deadline && writer.Matched() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (writer.Matched() == 0) {
        std::cerr << "rt/voice/tts_request 没有匹配到 Voice。核对 domain 和网卡。\n";
        return 1;
    }

    dds_::VoiceHeader_ header;
    header.source(source);
    header.timestamp(UnixMs());

    dds_::TTSRequest_ request;
    request.header(header);
    request.speak_id(speak_id);
    request.text(text);
    request.seq_num(0);
    request.priority(dds_::PriorityLevel::PRIORITY_NORMAL);
    request.interrupt(false);
    request.is_final(true);
    request.user_data("{}");
    request.language("zh");
    request.reply_to_command_id("");

    if (!writer.Write(request)) {
        std::cerr << "Write 失败\n";
        return 1;
    }
    std::cout << "已发送 TTS，speak_id=" << speak_id << " text=" << text << "\n";

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline && terminal.load() < 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (terminal.load() < 0) {
        std::cerr << "超时，没有收到播放结束事件\n";
        return 1;
    }
    const auto event = static_cast<dds_::OutputEventType>(terminal.load());
    std::cout << "结束: " << EventName(event) << "\n";
    return event == dds_::OutputEventType::PLAYBACK_COMPLETED ? 0 : 1;
}
