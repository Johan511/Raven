#pragma once

#include <protobuf_messages.hpp>
#include <utilities.hpp>

namespace rvn
{
class SubscriptionBuilder
{
    static std::uint64_t subscribeIDCounter;
    /*
        uint64 SubscribeID = 1;
        uint64 TrackAlias = 2;
        string TrackNamespace = 3;
        string TrackName = 4;
        uint32 SubscriberPriority = 5; // should be 8 bits
        uint32 GroupOrder = 6; // should be 8 bits
        SubscribeFilter FilterType = 7;
        optional uint64 StartGroup = 8;
        optional uint64 StartObject = 9;
        optional uint64 EndGroup = 10;
        optional uint64 EndObject = 11;
    */

    protobuf_messages::SubscribeMessage subscribeMessage;

    enum class SetElements
    {
        TrackAlias,
        TrackNamespace,
        TrackName,
        SubscriberPriority,
        GroupOrder,
        Range // Filter type and object, group start/ends
    };
    std::uint16_t setElementsCounter;

    static constexpr std::uint16_t full_value()
    {
        return 0b111111;
    }


    void set_defaults()
    {
        subscribeMessage.set_subscribeid(subscribeIDCounter++);
        setElementsCounter = 0;
    }

public:
    SubscriptionBuilder()
    {
        set_defaults();
    }


    SubscriptionBuilder& set_track_alias(std::uint64_t trackAlias)
    {
        subscribeMessage.set_trackalias(trackAlias);
        setElementsCounter |= (1 << utils::to_underlying(SetElements::TrackAlias));
        return *this;
    }

    SubscriptionBuilder& set_track_namespace(const std::string& trackNamespace)
    {
        subscribeMessage.set_tracknamespace(trackNamespace);
        setElementsCounter |= (1 << utils::to_underlying(SetElements::TrackNamespace));
        return *this;
    }

    SubscriptionBuilder& set_track_name(const std::string& trackName)
    {
        subscribeMessage.set_trackname(trackName);
        setElementsCounter |= (1 << utils::to_underlying(SetElements::TrackName));
        return *this;
    }

    SubscriptionBuilder& set_subscriber_priority(std::uint64_t subscriberPriority)
    {
        subscribeMessage.set_subscriberpriority(subscriberPriority);
        setElementsCounter |=
        (1 << utils::to_underlying(SetElements::SubscriberPriority));
        return *this;
    }

    SubscriptionBuilder& set_group_order(std::uint64_t groupOrder)
    {
        subscribeMessage.set_grouporder(groupOrder);
        setElementsCounter |= (1 << utils::to_underlying(SetElements::GroupOrder));
        return *this;
    }

    struct Filter
    {
        using protobuf_messages::SubscribeFilter::AbsoluteRange;
        using protobuf_messages::SubscribeFilter::AbsoluteStart;
        using protobuf_messages::SubscribeFilter::LatestGroup;
        using protobuf_messages::SubscribeFilter::LatestObject;
    }; // namespace Filter

    template <protobuf_messages::SubscribeFilter filter, typename... Args>
    SubscriptionBuilder& set_data_range(Args... args);

    protobuf_messages::SubscribeMessage build()
    {
        utils::ASSERT_LOG_THROW(setElementsCounter == 0b111111,
                                "Not all elements set in subscribe message");
        auto message = std::move(subscribeMessage);
        set_defaults();

        return message;
    }
};


}; // namespace rvn
