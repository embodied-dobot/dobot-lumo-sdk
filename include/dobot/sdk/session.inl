#pragma once

#include <utility>

#include <fastdds/dds/core/status/PublicationMatchedStatus.hpp>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>

namespace dobot::sdk {

template <typename Msg>
struct Publisher<Msg>::State {
    mutable std::mutex mu;
    eprosima::fastdds::dds::DataWriter* writer = nullptr;
    std::function<bool(const Msg&)> write;
};

template <typename Msg>
bool Publisher<Msg>::Write(const Msg& msg) const {
    if (!state_) {
        return false;
    }
    Msg copy = msg;
    std::function<bool(const Msg&)> write;
    {
        std::lock_guard<std::mutex> lock(state_->mu);
        write = state_->write;
    }
    if (!write) {
        return false;
    }
    return write(copy);
}

template <typename Msg>
int Publisher<Msg>::Matched() const {
    if (!state_) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(state_->mu);
    if (state_->writer == nullptr) {
        return 0;
    }
    eprosima::fastdds::dds::PublicationMatchedStatus status;
    if (state_->writer->get_publication_matched_status(status) != eprosima::fastdds::dds::RETCODE_OK) {
        return 0;
    }
    return status.current_count;
}

template <typename PubSubType>
Subscriber<typename PubSubType::type> Session::Subscribe(const std::string& topic) {
    using Msg = typename PubSubType::type;
    Subscriber<Msg> subscriber;
    std::lock_guard<std::mutex> lock(mu_);
    if (!open_.load() || !framework_) {
        return subscriber;
    }

    const std::type_index type(typeid(PubSubType));
    if (const auto found = subscriptions_.find(type); found != subscriptions_.end()) {
        if (found->second.topic != topic) {
            DOBOT_ERROR("A message type can only subscribe to one topic: already subscribed to {}, cannot also subscribe to {}", found->second.topic, topic);
            return subscriber;
        }
        subscriber.state_ = std::static_pointer_cast<detail::SubState<Msg>>(found->second.state);
        return subscriber;
    }

    if (!framework_->registerType<PubSubType>()) {
        DOBOT_ERROR("registerType failed, topic {}", topic);
        return subscriber;
    }

    auto state = std::make_shared<detail::SubState<Msg>>();
    auto endpoint = std::make_shared<ReaderEndpoint>();
    framework_->setDdsRefConsumerCallback<PubSubType>([this, state](const Msg& msg) {
        if (!open_.load()) {
            return;
        }
        std::function<void(const Msg&)> callback;
        {
            std::lock_guard<std::mutex> sample_lock(state->mu);
            state->latest = msg;
            state->cv.notify_all();
            callback = state->on;
        }
        if (!callback) {
            return;
        }
        try {
            callback(msg);
        } catch (...) {
            // An exception in the callback stops at this worker and does not block later samples.
        }
    });

    if (!framework_->registerTopicForType<PubSubType>(topic, dobot::dds::TopicMode::kReaderOnly)) {
        DOBOT_ERROR("registerTopic failed, topic {}", topic);
        return subscriber;
    }
    endpoint->reader = framework_->getGenericDdsManager()->template createReaderForType<PubSubType>(topic);
    if (endpoint->reader == nullptr) {
        DOBOT_ERROR("Failed to create reader, topic {}", topic);
        return subscriber;
    }

    state->matched_count = [endpoint] {
        std::lock_guard<std::mutex> endpoint_lock(endpoint->mu);
        if (endpoint->reader == nullptr) {
            return 0;
        }
        eprosima::fastdds::dds::SubscriptionMatchedStatus status;
        if (endpoint->reader->get_subscription_matched_status(status) != eprosima::fastdds::dds::RETCODE_OK) {
            return 0;
        }
        return status.current_count;
    };
    close_endpoints_.push_back([endpoint] {
        std::lock_guard<std::mutex> endpoint_lock(endpoint->mu);
        endpoint->reader = nullptr;
    });
    subscriptions_.insert_or_assign(type, SubEntry{topic, state});
    subscriber.state_ = std::move(state);
    return subscriber;
}

template <typename PubSubType>
Publisher<typename PubSubType::type> Session::Publish(const std::string& topic) {
    using Msg = typename PubSubType::type;
    Publisher<Msg> publisher;
    std::lock_guard<std::mutex> lock(mu_);
    if (!open_.load() || !framework_) {
        return publisher;
    }

    const std::type_index type(typeid(PubSubType));
    if (const auto found = publications_.find(topic); found != publications_.end()) {
        if (found->second.type != type) {
            DOBOT_ERROR("Topic {} is already bound to a different message type", topic);
            return publisher;
        }
        publisher.state_ = std::static_pointer_cast<typename Publisher<Msg>::State>(found->second.state);
        return publisher;
    }

    if (!framework_->registerType<PubSubType>()) {
        DOBOT_ERROR("registerType failed, topic {}", topic);
        return publisher;
    }
    if (!framework_->registerTopicForType<PubSubType>(topic, dobot::dds::TopicMode::kWriterOnly)) {
        DOBOT_ERROR("registerTopic failed, topic {}", topic);
        return publisher;
    }

    auto* writer = framework_->getGenericDdsManager()->template createWriterForType<PubSubType>(topic);
    if (writer == nullptr) {
        DOBOT_ERROR("Failed to create writer, topic {}", topic);
        return publisher;
    }

    auto state = std::make_shared<typename Publisher<Msg>::State>();
    state->writer = writer;
    std::shared_ptr<dobot::dds::DdsClientFramework> framework = framework_;
    state->write = [framework, topic](const Msg& msg) { return framework->template publish<PubSubType>(topic, msg); };
    close_endpoints_.push_back([state] {
        std::lock_guard<std::mutex> writer_lock(state->mu);
        state->writer = nullptr;
        state->write = nullptr;
    });
    publications_.insert_or_assign(topic, PubEntry{type, state});
    publisher.state_ = std::move(state);
    return publisher;
}

}  // namespace dobot::sdk
