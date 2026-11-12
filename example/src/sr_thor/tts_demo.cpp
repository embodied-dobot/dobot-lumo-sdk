// SR_thor text-to-speech example. Sends a block of text to rt/voice/tts_request
// and subscribes to rt/voice/output_event until playback ends or is rejected.
#include "dobot/robot/sr_thor/topics.hpp"
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
    std::cerr << "Usage: " << argv0
              << " [--text <text>] [--source agent] [--seconds 15]"
              << " [--on-robot] [--domain 0] [--nic <nic>]\n"
              << "Sends a block of text to rt/voice/tts_request and subscribes to rt/voice/output_event.\n"
              << "source must be a name that the Voice source policy allows for TTS; the documented example is agent.\n"
              << "DDS domain defaults to 0. All network interfaces are used by default; with several, add --nic.\n";
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
    std::string text = "Status is normal.";
    std::string source = "agent";
    int seconds = 15;
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
            std::cerr << "Unknown argument: " << arg << "\n";
            Usage(argv[0]);
            return 2;
        }
    }

    dobot::sdk::Session session;
    std::string error;
    if (!session.Init(dobot::robot::sr_thor::Profile(), config, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    auto events = session.Subscribe<dds_::VoiceOutputEvent_PubSubType>(
        dobot::robot::sr_thor::kTopicVoiceOutputEvent);
    auto writer = session.Publish<dds_::TTSRequest_PubSubType>(dobot::robot::sr_thor::kTopicVoiceTtsRequest);
    if (!events.valid() || !writer.valid()) {
        std::cerr << "Failed to create subscriber or publisher\n";
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
        std::cerr << "rt/voice/tts_request did not match Voice. Check the domain and network interface.\n";
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
        std::cerr << "Write failed\n";
        return 1;
    }
    std::cout << "TTS sent, speak_id=" << speak_id << " text=" << text << "\n";

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline && terminal.load() < 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (terminal.load() < 0) {
        std::cerr << "Timed out; no playback-end event received\n";
        return 1;
    }
    const auto event = static_cast<dds_::OutputEventType>(terminal.load());
    std::cout << "Finished: " << EventName(event) << "\n";
    return event == dds_::OutputEventType::PLAYBACK_COMPLETED ? 0 : 1;
}
