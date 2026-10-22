// FastDDS 通信骨架：单 Participant、多 PubSubType、多 Topic 的通用传输层。
#pragma once

#include <transport/dds_support.hpp>
#include <transport/fastdds_effective_configuration_log.hpp>
#include <transport/ordered_receive_lane.hpp>

#include <dobot_log.hpp>

#include <fastdds/dds/core/ReturnCode.hpp>
#include <fastdds/dds/core/status/BaseStatus.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

/*
    DomainParticipant：域参与者，管理Publisher、Subscriber和Topic
    Publisher：数据发布者，创建DataWriter。
    Subscriber：数据订阅者，创建DataReader。
    Topic：数据主题，关联数据类型和名称。
    DataWriter：实际写入数据的对象。
    DataReader：实际读取数据的对象。
    TypeSupport：数据类型支持，用于注册类型。
    它们之间的关系是：DomainParticipant创建Publisher和Subscriber。Publisher创建DataWriter，Subscriber创建DataReader。DataWriter和DataReader都与Topic关联。Topic与TypeSupport关联，TypeSupport定义了数据的结构

一对多关系：

    1个 DomainParticipant → N个 Publisher/Subscriber
    1个 Publisher → N个 DataWriter
    1个 Subscriber → N个 DataReader
    1个 Topic → N个 DataWriter + N个 DataReader

绑定关系：

    DataWriter 必须绑定到一个 Topic 和一个 Publisher
    DataReader 必须绑定到一个 Topic 和一个 Subscriber
    Topic 必须绑定到一个 TypeSupport

通信匹配：

    DataWriter 和 DataReader 通过 Topic 名称和数据类型匹配
    匹配的 Writer/Reader 才能进行数据交换

    创建顺序（必须遵循）：

    DomainParticipantFactory::get_instance()
    create_participant()→ participant_
    register_type()→ support
    create_topic()→ topics_
    create_publisher()→ publisher_
    create_subscriber()→ subscriber_
    create_datawriter()→ writers_
    create_datareader()→ readers_

销毁顺序（逆序）：

    删除 readers_→ delete_datareader()
    删除 writers_→ delete_datawriter()
    删除 subscriber_→ delete_subscriber()
    删除 publisher_→ delete_publisher()
    删除 topics_→ delete_topic()
    删除 participant_→ delete_participant()

*/
namespace dobot::dds
{

    inline constexpr char KDefaultParticipantProfile[] = "default_participant_profile";
    inline constexpr char kDefaultXmlWriterQosProfile[] = "default_writer_profile";
    inline constexpr char kDefaultXmlReaderQosProfile[] = "default_reader_profile";
    inline constexpr char kXmlWriterProfileSuffix[] = ".writer";
    inline constexpr char kXmlReaderProfileSuffix[] = ".reader";
    // 未显式指定 profile 时使用 topic+".writer" / topic+".reader"；XML 中无对应项则回退默认 QoS

    inline std::string resolveXmlQosProfile(const std::string &topic_name, const std::string &qos_profile, const char *suffix)
    {
        if (qos_profile.empty() || qos_profile == kDefaultXmlWriterQosProfile || qos_profile == kDefaultXmlReaderQosProfile) {
            return topic_name + suffix;
        }
        return qos_profile;
    }

    enum class ReceiveDispatchMode : uint8_t {
        kSync = 0,          // Listener 线程直接 take + 回调
        kAsyncOrdered = 1,  // take 入 lane，worker 有序 dispatch
    };

    class WriterPublicationListener : public eprosima::fastdds::dds::DataWriterListener
    {
    public:
        explicit WriterPublicationListener(std::atomic<bool> *matched) noexcept : matched_(matched) {}

        void on_publication_matched(eprosima::fastdds::dds::DataWriter *, const eprosima::fastdds::dds::PublicationMatchedStatus &info) override
        {
            if (matched_) { matched_->store(info.current_count > 0, std::memory_order_release); }
        }

    private:
        std::atomic<bool> *matched_{nullptr};
    };

    struct WriterSlot {
        eprosima::fastdds::dds::DataWriter *writer{nullptr};
        std::unique_ptr<std::atomic<bool>> publication_matched{std::make_unique<std::atomic<bool>>(false)};
        std::unique_ptr<WriterPublicationListener> listener;
#if 0  // 应用层 publish 队列暂禁用
        std::shared_ptr<void> publish_lane_owner;
        std::atomic<void *> publish_lane{nullptr};  // publishAsync 热路径直接 load，避免 map 查找
#endif
    };

    // 生命周期：initialize -> registerType/Topic/Callback -> start -> publish/stop -> ShutDown
    // is_async=true：订阅走 ordered lane；is_async=false：订阅在 listener 内同步回调
    // 发布均直接 DataWriter::write()；异步发送由 Fast DDS publishMode ASYNCHRONOUS 承担
    class GenericDDSManager : public DDSDataListener<GenericDDSManager>
    {
        friend class DDSDataListener<GenericDDSManager>;

    public:
        enum class TopicMode : uint8_t {
            kWriterOnly = 1,
            kReaderOnly = 2,
            kReaderWriter = 3,
        };

        explicit GenericDDSManager(bool is_async, std::string fastdds_xml_profiles_file = {}, size_t ordered_lane_capacity = 4096, size_t ordered_worker_count = 4)
          : is_async_(is_async)
          , xml_profiles_file_(std::move(fastdds_xml_profiles_file))
          , ordered_lane_capacity_(ordered_lane_capacity > 0 ? ordered_lane_capacity : 4096)
          , ordered_worker_count_(ordered_worker_count > 0 ? ordered_worker_count : 1)
          , ordered_receive_enabled_(is_async)
        {
        }

        ~GenericDDSManager() { ShutDown(); }

        GenericDDSManager(const GenericDDSManager &) = delete;
        GenericDDSManager &operator=(const GenericDDSManager &) = delete;

        bool initialize() noexcept
        {
            if (initialized_.load(std::memory_order_acquire)) { return true; }
            try {
                const bool xml_loaded = loadXmlProfiles();
                if (!xml_profiles_file_.empty() && !xml_loaded) { DOBOT_WARN("QoS XML not loaded (path={}), using Fast DDS defaults", xml_profiles_file_); }

                auto *factory = eprosima::fastdds::dds::DomainParticipantFactory::get_instance();

                eprosima::fastdds::dds::DomainParticipantQos pqos;
                auto ret = factory->get_participant_qos_from_profile(KDefaultParticipantProfile, pqos);
                if (ret != eprosima::fastdds::dds::RETCODE_OK) {
                    if (factory->get_default_participant_qos(pqos) != eprosima::fastdds::dds::RETCODE_OK) {
                        DOBOT_ERROR("get_default_participant_qos failed");
                        return false;
                    }
                }

                participant_ = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->create_participant_with_profile(KDefaultParticipantProfile);

                if (participant_) {
                    domain_id_ = participant_->get_domain_id();
                    DOBOT_INFO("participant created, profile={}, domain_id={}", KDefaultParticipantProfile, domain_id_);
                } else {
                    participant_ = factory->create_participant(static_cast<eprosima::fastdds::dds::DomainId_t>(domain_id_), pqos);

                    if (!participant_) {
                        DOBOT_ERROR("create_participant failed");
                        return false;
                    }
                    DOBOT_WARN("participant profile {} missing, using domain_id={}", KDefaultParticipantProfile, domain_id_);
                }

                if (!registerAllTypes()) {
                    factory->delete_participant(participant_);
                    participant_ = nullptr;
                    return false;
                }

                eprosima::fastdds::dds::PublisherQos pub_qos = eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT;
                participant_->get_default_publisher_qos(pub_qos);
                publisher_ = participant_->create_publisher(pub_qos);
                if (!publisher_) {
                    DOBOT_ERROR("create_publisher failed");
                    factory->delete_participant(participant_);
                    participant_ = nullptr;
                    return false;
                }

                eprosima::fastdds::dds::SubscriberQos sub_qos = eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT;
                participant_->get_default_subscriber_qos(sub_qos);
                subscriber_ = participant_->create_subscriber(sub_qos);
                if (!subscriber_) {
                    DOBOT_ERROR("create_subscriber failed");
                    participant_->delete_publisher(publisher_);
                    publisher_ = nullptr;
                    factory->delete_participant(participant_);
                    participant_ = nullptr;
                    return false;
                }

                initialized_.store(true, std::memory_order_release);
                if (ordered_receive_enabled_.load(std::memory_order_acquire)) { ensureReceiveDispatchPool(); }
                DOBOT_INFO("GenericDDSManager initialized, domain_id={}, is_async={}", participant_->get_domain_id(), is_async_);
                printFastDdsConfiguration();
                return true;
            } catch (const std::exception &e) {
                DOBOT_ERROR("GenericDDSManager initialize failed: {}", e.what());
                if (publisher_ && participant_) {
                    participant_->delete_publisher(publisher_);
                    publisher_ = nullptr;
                }
                if (subscriber_ && participant_) {
                    participant_->delete_subscriber(subscriber_);
                    subscriber_ = nullptr;
                }
                if (participant_) {
                    eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->delete_participant(participant_);
                    participant_ = nullptr;
                }
                return false;
            }
        }

        void ShutDown() noexcept
        {
            if (!initialized_.load(std::memory_order_acquire)) { return; }
            transport_started_.store(false, std::memory_order_release);
            stopDispatchPools();
            initialized_.store(false, std::memory_order_release);

            for (auto &[name, slot] : writers_) {
                (void)name;
                if (slot.writer) { publisher_->delete_datawriter(slot.writer); }
            }
            writers_.clear();
            topic_type_index_.clear();
            for (auto &[name, r] : readers_) {
                (void)name;
                subscriber_->delete_datareader(r);
            }
            readers_.clear();
            reader_type_indices_.clear();
            ordered_reader_handlers_.clear();
            receive_lane_controls_.clear();
#if 0  // 应用层 publish 队列暂禁用
            publish_lane_controls_.clear();
            publish_dispatch_pool_.reset();
            publish_pool_running_ = false;
#endif
            receive_dispatch_pool_.reset();
            receive_pool_running_ = false;
            for (auto &[name, t] : topics_) {
                (void)name;
                participant_->delete_topic(t);
            }
            topics_.clear();

            if (publisher_) {
                participant_->delete_publisher(publisher_);
                publisher_ = nullptr;
            }
            if (subscriber_) {
                participant_->delete_subscriber(subscriber_);
                subscriber_ = nullptr;
            }
            if (participant_) {
                eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->delete_participant(participant_);
                participant_ = nullptr;
            }

            DOBOT_INFO("GenericDDSManager shutdown");
        }

        bool isInitialized() const noexcept { return initialized_.load(std::memory_order_acquire); }

        /**
         * 完成类型/Topic/订阅回调等注册后调用，之后才允许 publish 与订阅侧用户回调。
         * 成功时会对当前已创建的各类 DataReader 调用一次与 on_data_available 相同的处理，以排空注册完成前已到达的队列样本。
         */
        bool start()
        {
            if (!initialized_.load(std::memory_order_acquire) || !participant_) {
                DOBOT_ERROR("GenericDDSManager::start: not initialized");
                return false;
            }
            transport_started_.store(true, std::memory_order_release);
            startDispatchPools();
            waitWritersPublicationMatchedAtStart();  // 冷路径等待 Reader 匹配
            flushPendingIncomingReads();             // 排空 start 前已到达的样本
            DOBOT_INFO("GenericDDSManager started, is_async={}", is_async_);
            return true;
        }

        void stop() noexcept
        {
            // publish 失败、订阅回调停止；Reader 内数据保留，可再次 start()
            transport_started_.store(false, std::memory_order_release);
            stopDispatchPools();
        }

        bool isStarted() const noexcept { return transport_started_.load(std::memory_order_acquire); }

        void setPublicationMatchTimeoutMs(int timeout_ms) noexcept { publication_match_timeout_ms_ = timeout_ms >= 0 ? timeout_ms : 0; }

        int publicationMatchTimeoutMs() const noexcept { return publication_match_timeout_ms_; }

        /** 已配置域 id；initialize() 后亦可使用 participant()->get_domain_id() 读实际域。 */
        uint32_t domainId() const noexcept { return domain_id_; }

        const std::string &xmlProfilesFile() const noexcept { return xml_profiles_file_; }

        void printFastDdsConfiguration() const
        {
            if (!initialized_.load(std::memory_order_acquire) || !participant_) { return; }
            std::unordered_map<std::string, eprosima::fastdds::dds::DataWriter *> writer_ptrs;
            writer_ptrs.reserve(writers_.size());
            for (const auto &kv : writers_) { writer_ptrs.emplace(kv.first, kv.second.writer); }
            const std::string text = detail::formatFastDdsConfiguration(participant_, publisher_, subscriber_, topics_, writer_ptrs, readers_, participant_->get_domain_id(), xml_profiles_file_);
            DOBOT_WARN("{}", text);
        }

        bool isAsync() const noexcept { return is_async_; }

        void configureAsyncReceive(bool use_ordered_dispatch) noexcept
        {
            // false: 同步 take；true: per-topic 有序 lane（默认）
            ordered_receive_enabled_.store(use_ordered_dispatch, std::memory_order_release);
            if (use_ordered_dispatch) {
                activateOrderedReadersForAllTypes();
                if (transport_started_.load(std::memory_order_acquire)) { startDispatchPools(); }
            }
        }

        ReceiveDispatchMode receiveDispatchMode() const noexcept { return ordered_receive_enabled_.load(std::memory_order_acquire) ? ReceiveDispatchMode::kAsyncOrdered : ReceiveDispatchMode::kSync; }

        size_t orderedLaneCapacity() const noexcept { return ordered_lane_capacity_; }

        size_t orderedWorkerCount() const noexcept { return ordered_worker_count_; }

        // --- 订阅：须先 registerType<PubSubType>；共享/独占样本各一套 API ---
        template <typename OtherPubSubType, typename Consumer>
        bool setSharedTypedSampleConsumer(Consumer &&consumer)
        {
            using OtherSample = typename DdsTraits<OtherPubSubType>::SampleType;
            static_assert(std::is_invocable_v<Consumer, const std::shared_ptr<OtherSample> &>, "consumer signature must be void(const std::shared_ptr<Sample>&)");

            const std::type_index idx(typeid(OtherPubSubType));
            const auto it = type_entries_.find(idx);
            if (it == type_entries_.end()) {
                DOBOT_ERROR("setSharedTypedSampleConsumer: type not registered {}", idx.name());
                return false;
            }
            attachTypedSampleHandler(it->second, std::function<void(const std::shared_ptr<OtherSample> &)>(std::forward<Consumer>(consumer)));
            return true;
        }

        template <typename OtherPubSubType, typename Consumer>
        bool setUniqueSampleConsumer(Consumer &&consumer)
        {
            using OtherSample = typename DdsTraits<OtherPubSubType>::SampleType;
            static_assert(std::is_invocable_v<Consumer, std::unique_ptr<OtherSample>>, "consumer signature must be void(std::unique_ptr<Sample>)");

            const std::type_index idx(typeid(OtherPubSubType));
            const auto it = type_entries_.find(idx);
            if (it == type_entries_.end()) {
                DOBOT_ERROR("setUniqueSampleConsumer: type not registered {}", idx.name());
                return false;
            }
            attachUniqueTypedSampleHandler<OtherSample>(it->second, std::forward<Consumer>(consumer));
            return true;
        }

        /**
         * 与 setSharedTypedSampleConsumer 相同前置条件；回调为 **const 样本引用**，内部仍从 DDS take 后按需拷贝/批处理，不经过 shared_ptr。
         */
        template <typename OtherPubSubType, typename Consumer>
        bool setSharedTypedSampleConsumerRef(Consumer &&consumer)
        {
            using OtherSample = typename DdsTraits<OtherPubSubType>::SampleType;
            static_assert(std::is_invocable_v<Consumer, const OtherSample &>, "consumer signature must be void(const Sample&)");

            const std::type_index idx(typeid(OtherPubSubType));
            const auto it = type_entries_.find(idx);
            if (it == type_entries_.end()) {
                DOBOT_ERROR("setSharedTypedSampleConsumerRef: type not registered {}", idx.name());
                return false;
            }
            attachSampleHandlerConstRef(it->second, std::function<void(const OtherSample &)>(std::forward<Consumer>(consumer)));
            return true;
        }

        // --- 类型注册 ---
        template <typename OtherPubSubType>
        bool registerType()
        {
            const std::type_index idx(typeid(OtherPubSubType));
            if (type_entries_.find(idx) != type_entries_.end()) { return true; }

            eprosima::fastdds::dds::TypeSupport support(new OtherPubSubType());
            if (!registerTypeInternal(idx, std::move(support))) { return false; }
            if (initialized_.load(std::memory_order_acquire) && participant_) {
                auto &entry = type_entries_.at(idx);
                if (entry.support.register_type(participant_) != eprosima::fastdds::dds::RETCODE_OK) {
                    DOBOT_ERROR("register_type failed: {}", idx.name());
                    type_entries_.erase(idx);
                    return false;
                }
            }

            DOBOT_INFO("registered type: {}", type_entries_.at(idx).type_name);
            return true;
        }

        template <typename OtherPubSubType>
        bool registerTopicForType(const std::string &topic_name, TopicMode mode = TopicMode::kReaderWriter, std::string writer_qos_profile = kDefaultXmlWriterQosProfile,
                                  std::string reader_qos_profile = kDefaultXmlReaderQosProfile)
        {
            bool ret = true;
            if (mode == TopicMode::kWriterOnly || mode == TopicMode::kReaderWriter) { ret = (createWriterForType<OtherPubSubType>(topic_name, std::move(writer_qos_profile)) != nullptr) && ret; }
            if (mode == TopicMode::kReaderOnly || mode == TopicMode::kReaderWriter) { ret = (createReaderForType<OtherPubSubType>(topic_name, std::move(reader_qos_profile)) != nullptr) && ret; }
            return ret;
        }

        bool unregisterTopic(const std::string &topic_name)
        {
            bool removed = false;

            if (auto w_it = writers_.find(topic_name); w_it != writers_.end()) {
                if (publisher_ && w_it->second.writer) { publisher_->delete_datawriter(w_it->second.writer); }
#if 0  // 应用层 publish 队列暂禁用
                w_it->second.publish_lane.store(nullptr, std::memory_order_release);
                w_it->second.publish_lane_owner.reset();
#endif
                writers_.erase(w_it);
                removed = true;
            }

            if (auto r_it = readers_.find(topic_name); r_it != readers_.end()) {
                if (subscriber_ && r_it->second) {
                    ordered_reader_handlers_.erase(r_it->second);
                    reader_type_indices_.erase(r_it->second);
                    subscriber_->delete_datareader(r_it->second);
                }
                readers_.erase(r_it);
                removed = true;
            }

            if (auto t_it = topics_.find(topic_name); t_it != topics_.end()) {
                if (participant_ && t_it->second) { participant_->delete_topic(t_it->second); }
                topics_.erase(t_it);
                removed = true;
            }

            topic_type_index_.erase(topic_name);
            return removed;
        }

        bool unregisterTopics(std::initializer_list<std::string> topics) { return unregisterTopicsRange(topics.begin(), topics.end()); }

        bool unregisterTopics(const std::vector<std::string> &topics) { return unregisterTopicsRange(topics.begin(), topics.end()); }

        // --- 端点（与 registerTopicForType 同一路径；需已成功 initialize）---
        template <typename OtherPubSubType>
        eprosima::fastdds::dds::DataWriter *createWriterForType(const std::string &topic_name, const std::string &qos_profile = kDefaultXmlWriterQosProfile)
        {
            if (auto it = writers_.find(topic_name); it != writers_.end()) { return it->second.writer; }
            const std::type_index idx(typeid(OtherPubSubType));
            auto *topic = getOrCreateTopic(topic_name, idx);
            if (!topic) { return nullptr; }

            const std::string profile = resolveXmlQosProfile(topic_name, qos_profile, kXmlWriterProfileSuffix);
            eprosima::fastdds::dds::DataWriterQos wqos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
            const bool qos_from_xml = publisher_->get_datawriter_qos_from_profile(profile, wqos) == eprosima::fastdds::dds::RETCODE_OK;
            if (!qos_from_xml) {
                wqos = eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
                DOBOT_WARN("writer QoS profile {} not found, using default", profile);
            }

            auto [it, inserted] = writers_.try_emplace(topic_name);
            if (!inserted) { return it->second.writer; }
            WriterSlot &slot = it->second;
            slot.listener = std::make_unique<WriterPublicationListener>(slot.publication_matched.get());
            slot.writer = publisher_->create_datawriter(topic, wqos, slot.listener.get());
            if (!slot.writer) {
                writers_.erase(it);
                DOBOT_ERROR("create_datawriter failed, topic={}", topic_name);
                return nullptr;
            }
            syncPublicationMatchedFromWriter(slot);
            topic_type_index_.insert_or_assign(topic_name, idx);
            DOBOT_INFO("DataWriter created, topic={}, type={}, qos={}", topic_name, type_entries_.at(idx).type_name, qos_from_xml ? profile : "default");
#if 0  // 应用层 publish 队列暂禁用
            if (ordered_receive_enabled_.load(std::memory_order_acquire)) { attachPublishLane<typename DdsTraits<OtherPubSubType>::SampleType>(topic_name, slot); }
#endif
            return slot.writer;
        }

        template <typename OtherPubSubType>
        eprosima::fastdds::dds::DataReader *createReaderForType(const std::string &topic_name, const std::string &qos_profile = kDefaultXmlReaderQosProfile)
        {
            if (auto it = readers_.find(topic_name); it != readers_.end()) { return it->second; }
            const std::type_index idx(typeid(OtherPubSubType));
            auto *topic = getOrCreateTopic(topic_name, idx);
            if (!topic) { return nullptr; }

            const std::string profile = resolveXmlQosProfile(topic_name, qos_profile, kXmlReaderProfileSuffix);
            eprosima::fastdds::dds::DataReaderQos rqos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
            const bool qos_from_xml = subscriber_->get_datareader_qos_from_profile(profile, rqos) == eprosima::fastdds::dds::RETCODE_OK;
            if (!qos_from_xml) {
                rqos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
                DOBOT_WARN("reader QoS profile {} not found, using default", profile);
            }
            auto *reader = subscriber_->create_datareader(topic, rqos, this);
            if (!reader) {
                DOBOT_ERROR("create_datareader failed, topic={}", topic_name);
                return nullptr;
            }
            readers_[topic_name] = reader;
            reader_type_indices_.insert_or_assign(reader, idx);
            auto &entry = type_entries_.at(idx);
            if (ordered_receive_enabled_.load(std::memory_order_acquire) && entry.ordered_attach) { entry.ordered_attach(topic_name, reader); }
            DOBOT_INFO("DataReader created, topic={}, type={}, qos={}", topic_name, entry.type_name, qos_from_xml ? profile : "default");
            return reader;
        }

        // --- 发布（须显式模板实参 publish<PubSubType>(...)）---
        template <typename OtherPubSubType>
        bool publish(const std::string &topic_name, const std::shared_ptr<typename DdsTraits<OtherPubSubType>::SampleType> &sample)
        {
            if (!transport_started_.load(std::memory_order_acquire)) {
                DOBOT_WARN("publish ignored: not started, topic={}", topic_name);
                return false;
            }
            auto it = writers_.find(topic_name);
            if (it == writers_.end() || !it->second.writer || !sample) {
                DOBOT_ERROR("publish failed: invalid writer or sample, topic={}", topic_name);
                return false;
            }
            if (publication_match_timeout_ms_ > 0 && !it->second.publication_matched->load(std::memory_order_acquire)) { return false; }
            // 热路径仅 atomic 检查 matched，阻塞等待只在 start() 冷路径
            auto rc = it->second.writer->write(static_cast<const void *>(sample.get()));
            return rc == eprosima::fastdds::dds::RETCODE_OK;
        }

        template <typename OtherPubSubType>
        bool publish(const std::string &topic_name, const typename DdsTraits<OtherPubSubType>::SampleType &sample)
        {
            if (!transport_started_.load(std::memory_order_acquire)) {
                DOBOT_WARN("publish ignored: not started, topic={}", topic_name);
                return false;
            }
            auto it = writers_.find(topic_name);
            if (it == writers_.end() || !it->second.writer) {
                DOBOT_ERROR("publish failed: invalid writer, topic={}", topic_name);
                return false;
            }
            if (publication_match_timeout_ms_ > 0 && !it->second.publication_matched->load(std::memory_order_acquire)) { return false; }
            auto rc = it->second.writer->write(static_cast<const void *>(&sample));
            return rc == eprosima::fastdds::dds::RETCODE_OK;
        }

        void handleDataAvailable(eprosima::fastdds::dds::DataReader *reader)
        {
            if (!reader) { return; }
            if (!transport_started_.load(std::memory_order_acquire)) { return; }
            // 异步模式优先走 reader -> handler 直映射，减少 type 查找
            if (ordered_receive_enabled_.load(std::memory_order_acquire)) {
                if (auto ord_it = ordered_reader_handlers_.find(reader); ord_it != ordered_reader_handlers_.end()) {
                    ord_it->second(reader);
                    return;
                }
            }
            if (auto key_it = reader_type_indices_.find(reader); key_it != reader_type_indices_.end()) {
                if (auto type_it = type_entries_.find(key_it->second); type_it != type_entries_.end() && type_it->second.reader_handler) {
                    type_it->second.reader_handler(reader);
                    return;
                }
            }
            DOBOT_WARN("DataReader has no handler, sample dropped");
        }

        void handleSubscriptionMatched(eprosima::fastdds::dds::DataReader *, const eprosima::fastdds::dds::SubscriptionMatchedStatus &) {}
        void handleDeadlineMissed(eprosima::fastdds::dds::DataReader *, const eprosima::fastdds::dds::RequestedDeadlineMissedStatus &) {}
        void handleIncompatibleQos(eprosima::fastdds::dds::DataReader *, const eprosima::fastdds::dds::RequestedIncompatibleQosStatus &) {}

        void handleSampleRejected(eprosima::fastdds::dds::DataReader *, const eprosima::fastdds::dds::SampleRejectedStatus &status)
        {
            DOBOT_WARN("SampleRejected total_count={} last_reason={}", status.total_count, static_cast<int>(status.last_reason));
        }

        void handleLivelinessChanged(eprosima::fastdds::dds::DataReader *, const eprosima::fastdds::dds::LivelinessChangedStatus &) {}

        void handleSampleLost(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::SampleLostStatus &status)
        {
            std::string topic = "unknown";
            if (reader) {
                if (const auto *td = reader->get_topicdescription()) {
                    if (td) { topic = td->get_name(); }
                }
            }

            DOBOT_WARN("SampleLost: topic={}, lost_since_last={}, lost_total={}", topic, status.total_count_change, status.total_count);
        }

    private:
        template <typename Iter>
        bool unregisterTopicsRange(Iter first, Iter last)
        {
            bool ret = true;
            for (; first != last; ++first) { ret = unregisterTopic(*first) && ret; }
            return ret;
        }

        struct TypeEntry {
            eprosima::fastdds::dds::TypeSupport support;
            std::string type_name;
            std::function<void(eprosima::fastdds::dds::DataReader *)> reader_handler;                       // sync 路径
            std::function<void(const std::string &, eprosima::fastdds::dds::DataReader *)> ordered_attach;  // async 路径
        };

        struct OrderedLaneControl {
            std::function<void()> start;
            std::function<void()> stop;
        };

#if 0  // 应用层 publish 队列暂禁用
        template <typename Sample>
        OrderedReceiveLane<Sample> *publishLaneForTopic(const std::string &topic_name) const noexcept
        {
            auto it = writers_.find(topic_name);
            if (it == writers_.end()) { return nullptr; }
            return static_cast<OrderedReceiveLane<Sample> *>(it->second.publish_lane.load(std::memory_order_acquire));
        }

        void ensurePublishDispatchPool()
        {
            if (!publish_dispatch_pool_) { publish_dispatch_pool_ = std::make_unique<OrderedDispatchPool>(ordered_worker_count_); }
        }

        template <typename Sample>
        void attachPublishLane(const std::string &topic_name, WriterSlot &slot)
        {
            if (slot.publish_lane.load(std::memory_order_acquire) != nullptr) { return; }

            std::lock_guard<std::mutex> lock(publish_lane_mutex_);
            if (slot.publish_lane.load(std::memory_order_acquire) != nullptr) { return; }
            if (!slot.writer) {
                DOBOT_ERROR("attachPublishLane: invalid writer, topic={}", topic_name);
                return;
            }

            const std::string pool_key = topic_name + "#pub";
            auto lane = std::make_shared<OrderedReceiveLane<Sample>>(
                ordered_lane_capacity_,
                [this, topic_name](Sample sample) {
                    // worker 线程执行 write
                    auto it = writers_.find(topic_name);
                    if (it == writers_.end() || !it->second.writer) { return; }
                    if (publication_match_timeout_ms_ > 0 && (!it->second.publication_matched || !it->second.publication_matched->load(std::memory_order_acquire))) { return; }
                    (void)it->second.writer->write(static_cast<const void *>(&sample));
                },
                "pub_" + topic_name);
            lane->setEnqueueWake(publish_dispatch_pool_->workerWakeNotifier(pool_key));
            publish_dispatch_pool_->registerLane(lane->makeDispatchSlot(), pool_key);
            publish_lane_controls_.push_back(OrderedLaneControl{[lane] { lane->startAccepting(); },
                                                                [lane] {
                                                                    lane->stopAccepting();
                                                                    lane->drainRemaining();
                                                                }});
            if (publish_pool_running_ && transport_started_.load(std::memory_order_acquire)) { lane->startAccepting(); }

            slot.publish_lane_owner = lane;
            slot.publish_lane.store(lane.get(), std::memory_order_release);
        }
#endif

        void ensureReceiveDispatchPool()
        {
            if (!receive_dispatch_pool_) { receive_dispatch_pool_ = std::make_unique<OrderedDispatchPool>(ordered_worker_count_); }
        }

        void activateOrderedReadersForAllTypes()
        {
            if (!ordered_receive_enabled_.load(std::memory_order_acquire)) { return; }
            for (auto &[type_idx, entry] : type_entries_) {
                (void)type_idx;
                activateOrderedReadersForEntry(entry);
            }
        }

        template <typename OtherSample, typename Fn>
        void forEachValidSample(eprosima::fastdds::dds::DataReader *reader, Fn &&fn)
        {
            OtherSample sample{};
            eprosima::fastdds::dds::SampleInfo info{};
            while (reader->take_next_sample(&sample, &info) == eprosima::fastdds::dds::RETCODE_OK) {
                if (!info.valid_data) { continue; }
                fn(std::move(sample));
            }
        }

        template <typename OtherSample, typename Callback>
        void attachOrderedReader(const std::string &topic_name, eprosima::fastdds::dds::DataReader *reader, Callback callback)
        {
            if (!reader) { return; }
            if (ordered_reader_handlers_.count(reader) > 0) { return; }

            ensureReceiveDispatchPool();
            auto lane = std::make_shared<OrderedReceiveLane<OtherSample>>(ordered_lane_capacity_, std::move(callback), "ord_" + topic_name);
            lane->setEnqueueWake(receive_dispatch_pool_->workerWakeNotifier(topic_name));
            receive_dispatch_pool_->registerLane(lane->makeDispatchSlot(), topic_name);
            receive_lane_controls_.push_back(OrderedLaneControl{[lane] { lane->startAccepting(); },
                                                                [lane] {
                                                                    lane->stopAccepting();
                                                                    lane->drainRemaining();
                                                                }});
            ordered_reader_handlers_[reader] = [lane](eprosima::fastdds::dds::DataReader *data_reader) {
                // Listener 线程：take 后入 lane，尽快返回
                OtherSample sample{};
                eprosima::fastdds::dds::SampleInfo info{};
                while (data_reader->take_next_sample(&sample, &info) == eprosima::fastdds::dds::RETCODE_OK) {
                    if (!info.valid_data) { continue; }
                    (void)lane->push(std::move(sample));
                }
            };
            if (receive_pool_running_ && transport_started_.load(std::memory_order_acquire)) { lane->startAccepting(); }
        }

        template <typename OtherSample, typename DeliverOne>
        void attachReaderHandler(TypeEntry &entry, DeliverOne deliver_one)
        {
            std::function<void(OtherSample)> deliver_fn = std::forward<DeliverOne>(deliver_one);

            entry.ordered_attach = [this, deliver_fn](const std::string &topic_name, eprosima::fastdds::dds::DataReader *reader) { attachOrderedReader<OtherSample>(topic_name, reader, deliver_fn); };

            entry.reader_handler = [this, deliver_fn = std::move(deliver_fn)](eprosima::fastdds::dds::DataReader *reader) mutable {
                forEachValidSample<OtherSample>(reader, [&](OtherSample s) { deliver_fn(std::move(s)); });
            };

            if (ordered_receive_enabled_.load(std::memory_order_acquire)) { activateOrderedReadersForEntry(entry); }
        }

        void activateOrderedReadersForEntry(TypeEntry &entry)
        {
            if (!entry.ordered_attach) { return; }
            for (const auto &[topic_name, reader] : readers_) {
                if (auto tit = reader_type_indices_.find(reader); tit != reader_type_indices_.end()) {
                    if (auto te = type_entries_.find(tit->second); te != type_entries_.end() && &te->second == &entry) { entry.ordered_attach(topic_name, reader); }
                }
            }
        }

        void startDispatchPools()
        {
            if (!ordered_receive_enabled_.load(std::memory_order_acquire)) { return; }
            ensureReceiveDispatchPool();
            if (!receive_pool_running_) {
                receive_dispatch_pool_->start();
                receive_pool_running_ = true;
            }
            for (auto &ctl : receive_lane_controls_) {
                if (ctl.start) { ctl.start(); }
            }
#if 0  // 应用层 publish 队列暂禁用
            ensurePublishDispatchPool();
            if (!publish_pool_running_) {
                publish_dispatch_pool_->start();
                publish_pool_running_ = true;
            }
            for (auto &ctl : publish_lane_controls_) {
                if (ctl.start) { ctl.start(); }
            }
#endif
        }

        void stopDispatchPools() noexcept
        {
            for (auto &ctl : receive_lane_controls_) {
                if (ctl.stop) { ctl.stop(); }
            }
            if (receive_dispatch_pool_ && receive_pool_running_) {
                receive_dispatch_pool_->stop();
                receive_pool_running_ = false;
            }
#if 0  // 应用层 publish 队列暂禁用
            for (auto &ctl : publish_lane_controls_) {
                if (ctl.stop) { ctl.stop(); }
            }
            if (publish_dispatch_pool_ && publish_pool_running_) {
                publish_dispatch_pool_->stop();
                publish_pool_running_ = false;
            }
#endif
        }

        template <typename OtherSample>
        void attachTypedSampleHandler(TypeEntry &entry, std::function<void(const std::shared_ptr<OtherSample> &)> cb)
        {
            attachReaderHandler<OtherSample>(entry, [cb = std::move(cb)](OtherSample s) { cb(std::make_shared<OtherSample>(std::move(s))); });
        }

        template <typename OtherSample>
        void attachSampleHandlerConstRef(TypeEntry &entry, std::function<void(const OtherSample &)> cb)
        {
            attachReaderHandler<OtherSample>(entry, [cb = std::move(cb)](OtherSample s) { cb(s); });
        }

        template <typename OtherSample, typename Consumer>
        void attachUniqueTypedSampleHandler(TypeEntry &entry, Consumer &&consumer)
        {
            attachReaderHandler<OtherSample>(entry, [cb = std::forward<Consumer>(consumer)](OtherSample s) mutable { cb(std::make_unique<OtherSample>(std::move(s))); });
        }

        void flushPendingIncomingReads()
        {
            for (const auto &kv : readers_) {
                if (kv.second) { handleDataAvailable(kv.second); }
            }
        }

        eprosima::fastdds::dds::Topic *getOrCreateTopic(const std::string &topic_name, std::type_index type_idx)
        {
            if (auto it = topics_.find(topic_name); it != topics_.end()) {
                const auto idx_it = topic_type_index_.find(topic_name);
                if (idx_it != topic_type_index_.end() && idx_it->second != type_idx) {
                    DOBOT_ERROR("topic {} bound to type {}, cannot rebind {}", topic_name, idx_it->second.name(), type_idx.name());
                    return nullptr;
                }
                return it->second;
            }
            auto type_it = type_entries_.find(type_idx);
            if (type_it == type_entries_.end()) {
                DOBOT_ERROR("type not registered: {} (topic={})", type_idx.name(), topic_name);
                return nullptr;
            }
            eprosima::fastdds::dds::TopicQos tqos = eprosima::fastdds::dds::TOPIC_QOS_DEFAULT;
            if (participant_->get_default_topic_qos(tqos) != eprosima::fastdds::dds::RETCODE_OK) { tqos = eprosima::fastdds::dds::TOPIC_QOS_DEFAULT; }
            auto *topic = participant_->create_topic(topic_name, type_it->second.type_name, tqos);
            if (!topic) {
                DOBOT_ERROR("create_topic failed: {}", topic_name);
                return nullptr;
            }
            topics_[topic_name] = topic;
            topic_type_index_.insert_or_assign(topic_name, type_idx);
            return topic;
        }

        bool registerTypeInternal(std::type_index idx, eprosima::fastdds::dds::TypeSupport support)
        {
            if (!support) {
                DOBOT_ERROR("registerTypeInternal: type support is null, type={}", idx.name());
                return false;
            }
            TypeEntry entry;
            entry.type_name = support.get_type_name();
            entry.support = std::move(support);
            type_entries_[idx] = std::move(entry);
            return true;
        }

        bool registerAllTypes()
        {
            for (auto &[key, entry] : type_entries_) {
                (void)key;
                if (entry.support.register_type(participant_) != eprosima::fastdds::dds::RETCODE_OK) {
                    DOBOT_ERROR("register_type failed: {}", entry.type_name);
                    return false;
                }
            }
            return true;
        }

        static void syncPublicationMatchedFromWriter(WriterSlot &slot)
        {
            if (!slot.writer) { return; }
            eprosima::fastdds::dds::PublicationMatchedStatus status{};
            if (slot.writer->get_publication_matched_status(status) == eprosima::fastdds::dds::RETCODE_OK && slot.publication_matched) {
                slot.publication_matched->store(status.current_count > 0, std::memory_order_release);
            }
        }

        void waitWritersPublicationMatchedAtStart()
        {
            if (publication_match_timeout_ms_ <= 0 || writers_.empty()) { return; }

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(publication_match_timeout_ms_);
            while (std::chrono::steady_clock::now() < deadline) {
                bool all_matched = true;
                for (auto &[topic_name, slot] : writers_) {
                    syncPublicationMatchedFromWriter(slot);
                    if (!slot.publication_matched || !slot.publication_matched->load(std::memory_order_acquire)) {
                        all_matched = false;
                        (void)topic_name;
                    }
                }
                if (all_matched) { return; }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            for (const auto &[topic_name, slot] : writers_) {
                if (!slot.publication_matched || !slot.publication_matched->load(std::memory_order_acquire)) {
                    DOBOT_WARN("DataWriter not matched, topic={}, timeout={}ms", topic_name, publication_match_timeout_ms_);
                }
            }
        }

        /** @return XML 已成功读入 ParticipantFactory 的为 true（未指定路径或未尝试加载为 false）。 */
        bool loadXmlProfiles() noexcept
        {
            if (xml_profiles_file_.empty()) {
                DOBOT_WARN("QoS XML path empty, using Fast DDS defaults");
                return false;
            }
            std::error_code ec;
            if (!std::filesystem::exists(xml_profiles_file_, ec)) {
                DOBOT_WARN("QoS XML not found: {}", xml_profiles_file_);
                return false;
            }

            const auto rc = eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->load_XML_profiles_file(xml_profiles_file_);
            if (rc != eprosima::fastdds::dds::RETCODE_OK) {
                DOBOT_WARN("load_XML_profiles_file failed, rc={}, path={}", static_cast<int>(rc), xml_profiles_file_);
                return false;
            }
            DOBOT_INFO("QoS XML loaded: {}", xml_profiles_file_);
            return true;
        }

        // --- 配置 ---
        bool is_async_{true};
        std::string xml_profiles_file_;
        uint32_t domain_id_{0};
        size_t ordered_lane_capacity_{4096};
        size_t ordered_worker_count_{4};
        std::atomic<bool> ordered_receive_enabled_{true};
        int publication_match_timeout_ms_{5000};
        std::unique_ptr<OrderedDispatchPool> receive_dispatch_pool_;
        bool receive_pool_running_{false};
        std::vector<OrderedLaneControl> receive_lane_controls_;
#if 0  // 应用层 publish 队列暂禁用
        std::mutex publish_lane_mutex_;
        std::unique_ptr<OrderedDispatchPool> publish_dispatch_pool_;
        bool publish_pool_running_{false};
        std::vector<OrderedLaneControl> publish_lane_controls_;
#endif
        std::unordered_map<eprosima::fastdds::dds::DataReader *, std::function<void(eprosima::fastdds::dds::DataReader *)>> ordered_reader_handlers_;

        // --- 类型表（键为 std::type_index，对应各 PubSubType）---
        std::unordered_map<std::type_index, TypeEntry> type_entries_;

        // --- DDS 实体 ---
        std::atomic<bool> transport_started_{false};
        std::atomic<bool> initialized_{false};
        eprosima::fastdds::dds::DomainParticipant *participant_{nullptr};
        eprosima::fastdds::dds::Publisher *publisher_{nullptr};
        eprosima::fastdds::dds::Subscriber *subscriber_{nullptr};
        std::unordered_map<std::string, eprosima::fastdds::dds::Topic *> topics_;
        std::unordered_map<std::string, WriterSlot> writers_;
        std::unordered_map<std::string, eprosima::fastdds::dds::DataReader *> readers_;
        std::unordered_map<std::string, std::type_index> topic_type_index_;
        std::unordered_map<eprosima::fastdds::dds::DataReader *, std::type_index> reader_type_indices_;
    };

}  // namespace dobot::dds
