// Fast DDS 通信骨架：类型萃取、DataReaderListener CRTP。
#pragma once

#include <fastdds/dds/subscriber/DataReaderListener.hpp>

namespace dobot::dds
{

    template <typename PubSubType>
    struct DdsTraits {
        using SampleType = typename PubSubType::type;
    };

    template <typename Derived>
    class DDSDataListener : public eprosima::fastdds::dds::DataReaderListener
    {
    public:
        void on_data_available(eprosima::fastdds::dds::DataReader *reader) override { self()->handleDataAvailable(reader); }

        void on_subscription_matched(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::SubscriptionMatchedStatus &info) override
        {
            self()->handleSubscriptionMatched(reader, info);
        }

        void on_requested_deadline_missed(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::RequestedDeadlineMissedStatus &s) override
        {
            self()->handleDeadlineMissed(reader, s);
        }

        void on_requested_incompatible_qos(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::RequestedIncompatibleQosStatus &s) override
        {
            self()->handleIncompatibleQos(reader, s);
        }

        void on_sample_rejected(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::SampleRejectedStatus &s) override { self()->handleSampleRejected(reader, s); }

        void on_liveliness_changed(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::LivelinessChangedStatus &s) override { self()->handleLivelinessChanged(reader, s); }

        void on_sample_lost(eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::SampleLostStatus &s) override { self()->handleSampleLost(reader, s); }

    private:
        Derived *self() noexcept { return static_cast<Derived *>(this); }
    };

}  // namespace dobot::dds
