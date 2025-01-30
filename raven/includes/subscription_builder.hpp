#pragma once

#include <serialization/messages.hpp>
#include <utilities.hpp>

namespace rvn
{
class SubscriptionBuilder
{
    static thread_local std::uint64_t subscribeIDCounter_;
    /*
        SUBSCRIBE Message {
          Type (i) = 0x3,
          Length (i),
          Subscribe ID (i),
          Track Alias (i),
          Track Namespace (tuple),
          Track Name Length (i),
          Track Name (..),
          Subscriber Priority (8),
          Group Order (8),
          Filter Type (i),
          [StartGroup (i),
           StartObject (i)],
          [EndGroup (i),
           EndObject (i)],
          Number of Parameters (i),
          Subscribe Parameters (..) ...
        }
    */

    depracated::messages::SubscribeMessage subscribeMessage_;

    enum class SetElements
    {
        TrackAlias,
        TrackNamespace,
        TrackName,
        SubscriberPriority,
        GroupOrder,
        Range // Filter type and object, group start/ends
    };
    std::uint16_t setElementsCounter_;
    constexpr std::uint64_t all_elements_set()
    {
        return 0b111111;
    }

    static constexpr std::uint16_t full_value()
    {
        return 0b111111;
    }


    void set_defaults()
    {
        subscribeMessage_.subscribeId_ = subscribeIDCounter_++;
        setElementsCounter_ = 0;
    }

public:
    SubscriptionBuilder()
    {
        set_defaults();
    }


    SubscriptionBuilder& set_track_alias(std::uint64_t trackAlias)
    {
        subscribeMessage_.trackAlias_ = trackAlias;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::TrackAlias));
        return *this;
    }

    SubscriptionBuilder& set_track_namespace(std::vector<std::string> trackNamespace)
    {
        subscribeMessage_.trackNamespace_ = std::move(trackNamespace);
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::TrackNamespace));
        return *this;
    }

    SubscriptionBuilder& set_track_name(std::string trackName)
    {
        subscribeMessage_.trackName_ = std::move(trackName);
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::TrackName));
        return *this;
    }

    SubscriptionBuilder& set_subscriber_priority(std::uint64_t subscriberPriority)
    {
        subscribeMessage_.subscriberPriority_ = subscriberPriority;
        setElementsCounter_ |=
        (1 << utils::to_underlying(SetElements::SubscriberPriority));
        return *this;
    }

    SubscriptionBuilder& set_group_order(std::uint64_t groupOrder)
    {
        subscribeMessage_.groupOrder_ = groupOrder;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::GroupOrder));
        return *this;
    }

    class Filter
    {
        friend SubscriptionBuilder;
        struct LatestGroup
        {
            static constexpr auto filterType =
            depracated::messages::SubscribeMessage::FilterType::LatestGroup;
        };
        struct LatestObject
        {
            static constexpr auto filterType =
            depracated::messages::SubscribeMessage::FilterType::LatestObject;
        };
        struct AbsoluteStart
        {
            static constexpr auto filterType =
            depracated::messages::SubscribeMessage::FilterType::AbsoluteStart;
        };
        struct AbsoluteRange
        {
            static constexpr auto filterType =
            depracated::messages::SubscribeMessage::FilterType::AbsoluteRange;
        };


    public:
        static constexpr auto latestGroup = LatestGroup{};
        static constexpr auto latestObject = LatestObject{};
        static constexpr auto absoluteStart = AbsoluteStart{};
        static constexpr auto absoluteRange = AbsoluteRange{};
    }; // namespace Filter


    SubscriptionBuilder& set_data_range(Filter::LatestGroup f)
    {
        subscribeMessage_.filterType_ = f.filterType;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }
    SubscriptionBuilder& set_data_range(Filter::LatestObject f)
    {
        subscribeMessage_.filterType_ = f.filterType;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }
    SubscriptionBuilder&
    set_data_range(Filter::AbsoluteStart f,
                   depracated::messages::SubscribeMessage::GroupObjectPair start)
    {
        subscribeMessage_.filterType_ = f.filterType;
        subscribeMessage_.start_ = start;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }


    SubscriptionBuilder&
    set_data_range(Filter::AbsoluteRange f,
                   depracated::messages::SubscribeMessage::GroupObjectPair start,
                   depracated::messages::SubscribeMessage::GroupObjectPair end)
    {
        subscribeMessage_.filterType_ = f.filterType;
        subscribeMessage_.start_ = start;
        subscribeMessage_.end_ = end;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }

    depracated::messages::SubscribeMessage build()
    {
        utils::ASSERT_LOG_THROW(setElementsCounter_ == all_elements_set(),
                                "Not all elements set in subscribe message",
                                setElementsCounter_);
        auto message = std::move(subscribeMessage_);
        set_defaults();

        return message;
    }
};


}; // namespace rvn
