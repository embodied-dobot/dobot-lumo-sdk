// 将 Fast DDS 当前生效的 domain / Publisher&Subscriber QoS / Topic 打印出来
#pragma once

#include <algorithm>
#include <fastdds/dds/core/Time_t.hpp>
#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/rtps/attributes/ResourceManagement.hpp>
#include <fastdds/rtps/common/Locator.hpp>
#include <fastdds/rtps/transport/UDPTransportDescriptor.hpp>
#include <fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.hpp>

#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace dobot::dds::detail
{
    namespace fds = eprosima::fastdds::dds;
    namespace frtps = eprosima::fastdds::rtps;

    inline std::string strDurability(fds::DurabilityQosPolicyKind_t k)
    {
        switch (k) {
            case fds::VOLATILE_DURABILITY_QOS:
                return "VOLATILE";
            case fds::TRANSIENT_LOCAL_DURABILITY_QOS:
                return "TRANSIENT_LOCAL";
            case fds::TRANSIENT_DURABILITY_QOS:
                return "TRANSIENT";
            case fds::PERSISTENT_DURABILITY_QOS:
                return "PERSISTENT";
            default:
                return "?";
        }
    }

    inline std::string strReliability(fds::ReliabilityQosPolicyKind k)
    {
        switch (k) {
            case fds::BEST_EFFORT_RELIABILITY_QOS:
                return "BEST_EFFORT";
            case fds::RELIABLE_RELIABILITY_QOS:
                return "RELIABLE";
            default:
                return "?";
        }
    }

    inline std::string strHistory(fds::HistoryQosPolicyKind k)
    {
        switch (k) {
            case fds::KEEP_LAST_HISTORY_QOS:
                return "KEEP_LAST";
            case fds::KEEP_ALL_HISTORY_QOS:
                return "KEEP_ALL";
            default:
                return "?";
        }
    }

    inline std::string strLiveliness(fds::LivelinessQosPolicyKind k)
    {
        switch (k) {
            case fds::AUTOMATIC_LIVELINESS_QOS:
                return "AUTOMATIC";
            case fds::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS:
                return "MANUAL_BY_PARTICIPANT";
            case fds::MANUAL_BY_TOPIC_LIVELINESS_QOS:
                return "MANUAL_BY_TOPIC";
            default:
                return "?";
        }
    }

    inline std::string strOwnership(fds::OwnershipQosPolicyKind k)
    {
        switch (k) {
            case fds::SHARED_OWNERSHIP_QOS:
                return "SHARED";
            case fds::EXCLUSIVE_OWNERSHIP_QOS:
                return "EXCLUSIVE";
            default:
                return "?";
        }
    }

    inline std::string strDur(fds::Duration_t d)
    {
        if (fds::Time_t::is_infinite(d)) { return "inf"; }
        return std::to_string(d.seconds) + "s+" + std::to_string(d.nanosec) + "ns";
    }

    inline std::string strPublishMode(fds::PublishModeQosPolicyKind k)
    {
        switch (k) {
            case fds::SYNCHRONOUS_PUBLISH_MODE:
                return "SYNCHRONOUS";
            case fds::ASYNCHRONOUS_PUBLISH_MODE:
                return "ASYNCHRONOUS";
            default:
                return "?";
        }
    }

    inline std::string strDataSharing(fds::DataSharingKind k)
    {
        switch (k) {
            case fds::AUTO:
                return "AUTO";
            case fds::ON:
                return "ON";
            case fds::OFF:
                return "OFF";
            default:
                return "?";
        }
    }

    inline std::string strDestinationOrder(fds::DestinationOrderQosPolicyKind k)
    {
        switch (k) {
            case fds::BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS:
                return "BY_RECEPTION";
            case fds::BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS:
                return "BY_SOURCE";
            default:
                return "?";
        }
    }

    inline std::string strHistoryMemoryPolicy(frtps::MemoryManagementPolicy_t k)
    {
        switch (k) {
            case frtps::PREALLOCATED_MEMORY_MODE:
                return "PREALLOCATED";
            case frtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE:
                return "PREALLOCATED_WITH_REALLOC";
            case frtps::DYNAMIC_RESERVE_MEMORY_MODE:
                return "DYNAMIC_RESERVE";
            case frtps::DYNAMIC_REUSABLE_MEMORY_MODE:
                return "DYNAMIC_REUSABLE";
            default:
                return "?";
        }
    }

    inline void appendResourceLimits(std::ostream &o, const fds::ResourceLimitsQosPolicy &r, const char *indent)
    {
        o << indent << "resource_limits: max_samples=" << r.max_samples << " max_instances=" << r.max_instances << " max_samples_per_instance=" << r.max_samples_per_instance
          << " allocated_samples=" << r.allocated_samples << " extra_samples=" << r.extra_samples << "\n";
    }

    inline void appendDisablePositiveAcks(std::ostream &o, const fds::DisablePositiveACKsQosPolicy &p, const char *indent)
    {
        o << indent << "disable_positive_acks: enabled=" << p.enabled << " duration=" << strDur(p.duration) << "\n";
    }

    inline void appendDataSharing(std::ostream &o, const fds::DataSharingQosPolicy &ds, const char *indent)
    {
        o << indent << "data_sharing: kind=" << strDataSharing(ds.kind());
        if (!ds.shm_directory().empty()) { o << " shm_dir=" << ds.shm_directory(); }
        o << "\n";
    }

    inline std::string locatorKindName(int32_t kind)
    {
        if (kind == LOCATOR_KIND_UDPv4) { return "UDPv4"; }
        if (kind == LOCATOR_KIND_UDPv6) { return "UDPv6"; }
        if (kind == LOCATOR_KIND_TCPv4) { return "TCPv4"; }
        if (kind == LOCATOR_KIND_TCPv6) { return "TCPv6"; }
        if (kind == LOCATOR_KIND_SHM) { return "SHM"; }
        return "kind=" + std::to_string(kind);
    }

    /** 简要说明传输与发现：内置传输开关、用户 transport 数量、发现协议、默认单播 Locator 类型。 */
    inline std::string summarizeCommunication(const fds::DomainParticipantQos &pqos)
    {
        const auto &tc = pqos.transport();
        const auto &wp = pqos.wire_protocol();
        const auto &disc = wp.builtin.discovery_config;

        std::ostringstream o;
        o << "discovery=" << disc.discoveryProtocol;
        o << ", lease=" << strDur(disc.leaseDuration) << ", announce=" << strDur(disc.leaseDuration_announcementperiod);
        o << ", builtin_transports=" << (tc.use_builtin_transports ? "on" : "off");
        o << ", user_transports=" << tc.user_transports.size();

        std::set<std::string> kinds;
        for (const auto &loc : wp.default_unicast_locator_list) { kinds.insert(locatorKindName(loc.kind)); }
        for (const auto &loc : wp.default_multicast_locator_list) { kinds.insert(locatorKindName(loc.kind)); }
        if (!kinds.empty()) {
            o << ", default_locator_kinds=[";
            bool first = true;
            for (const auto &k : kinds) {
                if (!first) { o << ","; }
                first = false;
                o << k;
            }
            o << "]";
        }
        return o.str();
    }

    inline void appendUserTransports(std::ostream &o, const fds::DomainParticipantQos &pqos)
    {
        const auto &tc = pqos.transport();
        o << "  use_builtin_transports=" << (tc.use_builtin_transports ? "on" : "off") << " user_transports=" << tc.user_transports.size() << "\n";
        for (const auto &t : tc.user_transports) {
            if (!t) { continue; }
            if (const auto *shm = dynamic_cast<const frtps::SharedMemTransportDescriptor *>(t.get())) {
                o << "  SHM: maxMessageSize=" << shm->maxMessageSize << " segment_size=" << shm->segment_size() << "\n";
            } else if (const auto *udp = dynamic_cast<const frtps::UDPTransportDescriptor *>(t.get())) {
                o << "  UDP: maxMessageSize=" << udp->maxMessageSize << " sendBuffer=" << udp->sendBufferSize << " recvBuffer=" << udp->receiveBufferSize
                  << " non_blocking_send=" << udp->non_blocking_send << "\n";
            } else {
                o << "  transport: maxMessageSize=" << t->maxMessageSize << "\n";
            }
        }
    }

    inline const char *strPresentationScope(fds::PresentationQosPolicyAccessScopeKind s)
    {
        switch (s) {
            case fds::INSTANCE_PRESENTATION_QOS:
                return "INSTANCE";
            case fds::TOPIC_PRESENTATION_QOS:
                return "TOPIC";
            case fds::GROUP_PRESENTATION_QOS:
                return "GROUP";
            default:
                return "?";
        }
    }

    inline void appendPublisherQosText(std::ostream &o, const fds::PublisherQos &q)
    {
        const auto &pr = q.presentation();
        o << "  presentation: scope=" << strPresentationScope(pr.access_scope) << " coherent=" << pr.coherent_access << " ordered=" << pr.ordered_access << "\n";
        o << "  partition: [";
        bool first = true;
        for (const auto &n : q.partition().names()) {
            if (!first) { o << ", "; }
            first = false;
            o << n;
        }
        o << "]\n";
        o << "  entity_factory.autoenable: " << q.entity_factory().autoenable_created_entities << "\n";
    }

    inline void appendSubscriberQosText(std::ostream &o, const fds::SubscriberQos &q)
    {
        const auto &pr = q.presentation();
        o << "  presentation: scope=" << strPresentationScope(pr.access_scope) << " coherent=" << pr.coherent_access << " ordered=" << pr.ordered_access << "\n";
        o << "  partition: [";
        bool first = true;
        for (const auto &n : q.partition().names()) {
            if (!first) { o << ", "; }
            first = false;
            o << n;
        }
        o << "]\n";
        o << "  entity_factory.autoenable: " << q.entity_factory().autoenable_created_entities << "\n";
    }

    inline void appendTopicQosText(std::ostream &o, const fds::TopicQos &q)
    {
        o << "  durability=" << strDurability(q.durability().kind) << "\n";
        o << "  deadline.period=" << strDur(q.deadline().period) << "\n";
        o << "  latency_budget=" << strDur(q.latency_budget().duration) << "\n";
        o << "  liveliness: kind=" << strLiveliness(q.liveliness().kind) << " lease=" << strDur(q.liveliness().lease_duration) << " announce=" << strDur(q.liveliness().announcement_period) << "\n";
        o << "  reliability: kind=" << strReliability(q.reliability().kind) << " max_blocking=" << strDur(q.reliability().max_blocking_time) << "\n";
        o << "  destination_order=" << strDestinationOrder(q.destination_order().kind) << "\n";
        o << "  history: kind=" << strHistory(q.history().kind) << " depth=" << q.history().depth << "\n";
        appendResourceLimits(o, q.resource_limits(), "  ");
        o << "  lifespan=" << strDur(q.lifespan().duration) << "\n";
        o << "  ownership=" << strOwnership(q.ownership().kind) << "\n";
        o << "  transport_priority=" << q.transport_priority().value << "\n";
    }

    inline void appendDataWriterQosText(std::ostream &o, const fds::DataWriterQos &q)
    {
        o << "    durability=" << strDurability(q.durability().kind) << "\n";
        o << "    deadline.period=" << strDur(q.deadline().period) << "\n";
        o << "    latency_budget=" << strDur(q.latency_budget().duration) << "\n";
        o << "    liveliness: kind=" << strLiveliness(q.liveliness().kind) << " lease=" << strDur(q.liveliness().lease_duration) << " announce=" << strDur(q.liveliness().announcement_period) << "\n";
        o << "    reliability: kind=" << strReliability(q.reliability().kind) << " max_blocking=" << strDur(q.reliability().max_blocking_time) << "\n";
        o << "    destination_order=" << strDestinationOrder(q.destination_order().kind) << "\n";
        o << "    history: kind=" << strHistory(q.history().kind) << " depth=" << q.history().depth << "\n";
        appendResourceLimits(o, q.resource_limits(), "    ");
        o << "    lifespan=" << strDur(q.lifespan().duration) << "\n";
        o << "    ownership=" << strOwnership(q.ownership().kind) << " strength=" << q.ownership_strength().value << "\n";
        o << "    transport_priority=" << q.transport_priority().value << "\n";
        o << "    publish_mode: kind=" << strPublishMode(q.publish_mode().kind) << " flow_controller=" << q.publish_mode().flow_controller_name << "\n";
        appendDataSharing(o, q.data_sharing(), "    ");
        appendDisablePositiveAcks(o, q.reliable_writer_qos().disable_positive_acks, "    ");
        o << "    history_memory_policy=" << strHistoryMemoryPolicy(q.endpoint().history_memory_policy) << "\n";
        o << "    writer_data_lifecycle.autodispose=" << q.writer_data_lifecycle().autodispose_unregistered_instances << "\n";
        const auto &t = q.reliable_writer_qos().times;
        o << "    reliable_writer: hb_period=" << strDur(t.heartbeat_period) << " nack_response=" << strDur(t.nack_response_delay) << " nack_supression=" << strDur(t.nack_supression_duration)
          << " initial_hb=" << strDur(t.initial_heartbeat_delay) << " disable_hb_piggyback=" << q.reliable_writer_qos().disable_heartbeat_piggyback << "\n";
    }

    inline void appendDataReaderQosText(std::ostream &o, const fds::DataReaderQos &q)
    {
        o << "    durability=" << strDurability(q.durability().kind) << "\n";
        o << "    deadline.period=" << strDur(q.deadline().period) << "\n";
        o << "    latency_budget=" << strDur(q.latency_budget().duration) << "\n";
        o << "    liveliness: kind=" << strLiveliness(q.liveliness().kind) << " lease=" << strDur(q.liveliness().lease_duration) << " announce=" << strDur(q.liveliness().announcement_period) << "\n";
        o << "    reliability: kind=" << strReliability(q.reliability().kind) << " max_blocking=" << strDur(q.reliability().max_blocking_time) << "\n";
        o << "    destination_order=" << strDestinationOrder(q.destination_order().kind) << "\n";
        o << "    history: kind=" << strHistory(q.history().kind) << " depth=" << q.history().depth << "\n";
        appendResourceLimits(o, q.resource_limits(), "    ");
        o << "    lifespan=" << strDur(q.lifespan().duration) << "\n";
        o << "    ownership=" << strOwnership(q.ownership().kind) << "\n";
        o << "    time_based_filter=" << strDur(q.time_based_filter().minimum_separation) << "\n";
        o << "    expects_inline_qos=" << q.expects_inline_qos() << "\n";
        appendDataSharing(o, q.data_sharing(), "    ");
        appendDisablePositiveAcks(o, q.reliable_reader_qos().disable_positive_acks, "    ");
        o << "    history_memory_policy=" << strHistoryMemoryPolicy(q.endpoint().history_memory_policy) << "\n";
        o << "    max_samples_per_read=" << q.reader_resource_limits().max_samples_per_read << "\n";
        o << "    type_consistency=" << (q.type_consistency().m_kind == fds::ALLOW_TYPE_COERCION ? "ALLOW_COERCION" : "DISALLOW_COERCION") << "\n";
        const auto &t = q.reliable_reader_qos().times;
        o << "    reliable_reader: initial_acknack=" << strDur(t.initial_acknack_delay) << " hb_response=" << strDur(t.heartbeat_response_delay) << "\n";
    }

    inline std::string formatFastDdsConfiguration(const eprosima::fastdds::dds::DomainParticipant *participant, const eprosima::fastdds::dds::Publisher *publisher,
                                                  const eprosima::fastdds::dds::Subscriber *subscriber, const std::unordered_map<std::string, eprosima::fastdds::dds::Topic *> &topics,
                                                  const std::unordered_map<std::string, eprosima::fastdds::dds::DataWriter *> &writers,
                                                  const std::unordered_map<std::string, eprosima::fastdds::dds::DataReader *> &readers, uint32_t /*configured_domain_id*/,
                                                  const std::string &xml_profiles_file)
    {
        std::ostringstream out;
        if (!participant) {
            out << "Fast DDS: participant null\n";
            return out.str();
        }

        const fds::DomainParticipantQos &pqos = participant->get_qos();
        out << "Fast DDS 生效配置\n";
        out << "domain_id: " << participant->get_domain_id() << "\n";
        out << "participant_name: " << pqos.name().c_str() << "\n";
        if (!xml_profiles_file.empty()) { out << "xml: " << xml_profiles_file << "\n"; }
        out << "通信方式: " << summarizeCommunication(pqos) << "\n";
        out << "[Transport]\n";
        appendUserTransports(out, pqos);

        if (publisher) {
            out << "\n[Publisher QoS]\n";
            appendPublisherQosText(out, publisher->get_qos());
        }
        if (subscriber) {
            out << "\n[Subscriber QoS]\n";
            appendSubscriberQosText(out, subscriber->get_qos());
        }

        std::set<std::string> all_topics;
        for (const auto &kv : topics) { all_topics.insert(kv.first); }
        for (const auto &kv : writers) { all_topics.insert(kv.first); }
        for (const auto &kv : readers) { all_topics.insert(kv.first); }

        for (const auto &tn : all_topics) {
            out << "\n[Topic: " << tn << "]\n";
            auto tit = topics.find(tn);
            if (tit != topics.end() && tit->second) {
                const auto *tp = tit->second;
                out << "  name=" << tp->get_name() << " type=" << tp->get_type_name() << "\n";
                out << "  TopicQoS:\n";
                appendTopicQosText(out, tp->get_qos());
            } else {
                out << "  (Topic 对象未缓存，仅显示端点 QoS)\n";
            }

            auto wit = writers.find(tn);
            if (wit != writers.end() && wit->second) {
                out << "  DataWriterQoS:\n";
                appendDataWriterQosText(out, wit->second->get_qos());
            }

            auto rit = readers.find(tn);
            if (rit != readers.end() && rit->second) {
                out << "  DataReaderQoS:\n";
                appendDataReaderQosText(out, rit->second->get_qos());
            }
        }

        return out.str();
    }

}  // namespace dobot::dds::detail
