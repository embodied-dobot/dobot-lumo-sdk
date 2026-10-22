// DDS 应用入口：DdsClientFramework 组合 AsyncTaskExecutor +
// GenericDDSManager；各 PubSub 类型通过 registerType<...>() 注册。
#pragma once

#include <application/runtime.hpp>
#include <dobot_log.hpp>
#include <transport/generic_transport.hpp>

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <utility>

namespace dobot::dds
{

    using TopicMode = GenericDDSManager::TopicMode;

    // DDS 收发走 GenericDDSManager
    // 典型流程：initDDSManager -> registerType/registerTopic/setCallback -> start
    // -> publish is_async=true：订阅走 ordered lane；is_async=false：订阅在
    // listener 内同步回调
    class DdsClientFramework
    {
    public:
        explicit DdsClientFramework(size_t async_worker_count = 4, size_t max_pending = 5000, std::string fastdds_xml_profiles_file = {}, bool is_async = false)
          : fastdds_xml_profiles_file_(std::move(fastdds_xml_profiles_file))
          , async_worker_count_(async_worker_count > 0 ? async_worker_count : 1)
          , max_pending_(max_pending > 0 ? max_pending : 5000)
          , is_async_(is_async)
          , generic_dds_manager_(std::make_shared<GenericDDSManager>(is_async_, fastdds_xml_profiles_file_, max_pending_, async_worker_count_))
        {
            if (!generic_dds_manager_->initialize()) {
                DOBOT_ERROR("DdsClientFramework: GenericDDSManager initialize failed");
                throw std::runtime_error("DdsClientFramework: GenericDDSManager initialize failed");
            }
        }

        ~DdsClientFramework() noexcept { shutdown(); }
        DdsClientFramework(const DdsClientFramework &) = delete;
        DdsClientFramework &operator=(const DdsClientFramework &) = delete;

        std::shared_ptr<GenericDDSManager> &getGenericDdsManager() noexcept { return generic_dds_manager_; }

        template <typename OtherPubSubType>
        bool registerType()
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("registerType: manager not initialized");
                return false;
            }
            return generic_dds_manager_->registerType<OtherPubSubType>();
        }

        bool initDDSManager()
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("initDDSManager: manager not initialized");
                return false;
            }
            return generic_dds_manager_->initialize();
        }

        /** 打印当前 Participant / Topic / Writer / Reader 等生效配置（须已成功
         * initDDSManager）。 */
        void printDdsConfiguration() const
        {
            if (generic_dds_manager_) { generic_dds_manager_->printFastDdsConfiguration(); }
        }

        /** 与 GenericDDSManager::start() 相同：完成
         * Topic/回调注册后调用，之后才允许收发。 */
        bool start()
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("start: manager not initialized");
                return false;
            }
            return generic_dds_manager_->start();
        }

        template <typename OtherPubSubType>
        bool registerTopicForType(const std::string &topic_name, TopicMode mode = TopicMode::kReaderWriter, std::string writer_qos_profile = kDefaultXmlWriterQosProfile,
                                  std::string reader_qos_profile = kDefaultXmlReaderQosProfile)
        {
            if (!generic_dds_manager_) { return false; }
            return generic_dds_manager_->registerTopicForType<OtherPubSubType>(topic_name, mode, std::move(writer_qos_profile), std::move(reader_qos_profile));
        }

        /*******************一下接口是 对外开放的 注册 订阅消息回调的接口
         * *******************************************/
        // OtherPubSubType 是IDL的 xxxPubSubType类型，Consumer类型 必须为 void(const
        // OtherPubSubType::type&),注意: IDL中 xxxPubSubType -> OtherPubSubType::type
        // 是一一对应的
        template <typename OtherPubSubType, typename Consumer>
        void setDdsRefConsumerCallback(Consumer &&consumer)
        {
            // 回调 const Sample&，无 shared_ptr 分配，低延迟首选
            if (!generic_dds_manager_) {
                DOBOT_ERROR("setDdsRefConsumerCallback: manager not initialized");
                return;
            }
            generic_dds_manager_->setSharedTypedSampleConsumerRef<OtherPubSubType>(std::forward<Consumer>(consumer));
        }

        // OtherPubSubType 是IDL的 xxxPubSubType类型，Consumer类型 必须为 void(const
        // std::shared_ptr<OtherPubSubType::type>&),注意: IDL中 xxxPubSubType ->
        // OtherPubSubType::type 是一一对应的
        template <typename OtherPubSubType, typename Consumer>
        void setDdsSharedConsumerCallback(Consumer &&consumer)
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("setDdsSharedConsumerCallback: manager not initialized");
                return;
            }
            generic_dds_manager_->setSharedTypedSampleConsumer<OtherPubSubType>(std::forward<Consumer>(consumer));
        }

        // OtherPubSubType 是IDL的 xxxPubSubType类型，Consumer类型 必须为
        // void(std::unique_ptr<OtherPubSubType::type>),注意: IDL中 xxxPubSubType ->
        // OtherPubSubType::type 是一一对应的
        template <typename OtherPubSubType, typename Consumer>
        void setDdsUniqueConsumerCallback(Consumer &&consumer)
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("setDdsUniqueConsumerCallback: manager not initialized");
                return;
            }
            generic_dds_manager_->setUniqueSampleConsumer<OtherPubSubType>(std::forward<Consumer>(consumer));
        }
        /*************************************************************************************************************/

        /*****************************同步 发布 消息 接口
         * ***************************************************************/
        template <typename OtherPubSubType>
        bool publish(const std::string &topic_name, const std::shared_ptr<typename DdsTraits<OtherPubSubType>::SampleType> &sample)
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("publish: manager not initialized");
                return false;
            }
            return generic_dds_manager_->publish<OtherPubSubType>(topic_name, sample);
        }

        template <typename OtherPubSubType>
        bool publish(const std::string &topic_name, const typename DdsTraits<OtherPubSubType>::SampleType &sample)
        {
            if (!generic_dds_manager_) {
                DOBOT_ERROR("publish: manager not initialized");
                return false;
            }
            return generic_dds_manager_->publish<OtherPubSubType>(topic_name, sample);
        }
        /*************************************************************************************************************/

        ReceiveDispatchMode receiveDispatchMode() const noexcept { return generic_dds_manager_->receiveDispatchMode(); }

        bool isAsync() const noexcept { return is_async_; }

        void stopDdsTransport() noexcept
        {
            if (generic_dds_manager_) { generic_dds_manager_->stop(); }
        }

        bool isDdsStarted() const noexcept { return generic_dds_manager_ && generic_dds_manager_->isStarted(); }

        void setPublicationMatchTimeoutMs(int timeout_ms) noexcept
        {
            // 0 表示 publish 不检查 matched；>0 时 start() 冷路径等待，热路径仅读
            // atomic
            if (generic_dds_manager_) { generic_dds_manager_->setPublicationMatchTimeoutMs(timeout_ms); }
        }

        void shutdown() noexcept
        {
            // stop() 停止收发并关闭有序 dispatch 线程；再 ShutDown() 删 DDS 实体。
            if (generic_dds_manager_) { generic_dds_manager_->stop(); }
            if (generic_dds_manager_) { generic_dds_manager_->ShutDown(); }
        }

    private:
        std::string fastdds_xml_profiles_file_;
        size_t async_worker_count_;
        size_t max_pending_;
        bool is_async_{false};
        std::shared_ptr<GenericDDSManager> generic_dds_manager_;
    };

}  // namespace dobot::dds
