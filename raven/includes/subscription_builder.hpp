#pragma once

#include <atomic>
#include <serialization/messages.hpp>
#include <utilities.hpp>

namespace rvn
{
class SubscriptionBuilder
{
    static std::atomic_uint64_t subscribeIDCounter_;
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

    SubscribeMessage subscribeMessage_;

    enum class SetElements
    {
        TrackAlias,
        TrackNamespace,
        TrackName,
        SubscriberPriority,
        GroupOrder,
        Range // Filter type and object/group start/end
    };
    std::uint16_t setElementsCounter_;
    constexpr std::uint64_t all_elements_set()
    {
        return 0b111111;
    }

    void set_defaults()
    {
        subscribeMessage_.subscribeId_ =
        subscribeIDCounter_.fetch_add(1, std::memory_order_relaxed);
        setElementsCounter_ = 0;
    }

public:
    SubscriptionBuilder()
    {
        set_defaults();
    }


    SubscriptionBuilder& set_track_alias(TrackAlias trackAlias)
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
            static constexpr auto filterType = SubscribeMessage::FilterType::LatestGroup;
        };
        struct LatestObject
        {
            static constexpr auto filterType = SubscribeMessage::FilterType::LatestObject;
        };
        struct AbsoluteStart
        {
            static constexpr auto filterType = SubscribeMessage::FilterType::AbsoluteStart;
        };
        struct AbsoluteRange
        {
            static constexpr auto filterType = SubscribeMessage::FilterType::AbsoluteRange;
        };
        struct LatestPerGroupInTrack
        {
            static constexpr auto filterType = SubscribeMessage::FilterType::LatestPerGroupInTrack;
        };

    public:
        static constexpr auto latestGroup = LatestGroup{};
        static constexpr auto latestObject = LatestObject{};
        static constexpr auto absoluteStart = AbsoluteStart{};
        static constexpr auto absoluteRange = AbsoluteRange{};
        static constexpr auto latestPerGroupInTrack = LatestPerGroupInTrack{};
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
    set_data_range(Filter::AbsoluteStart f, SubscribeMessage::GroupObjectPair start)
    {
        subscribeMessage_.filterType_ = f.filterType;
        subscribeMessage_.start_ = start;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }


    SubscriptionBuilder& set_data_range(Filter::AbsoluteRange f,
                                        SubscribeMessage::GroupObjectPair start,
                                        SubscribeMessage::GroupObjectPair end)
    {
        subscribeMessage_.filterType_ = f.filterType;
        subscribeMessage_.start_ = start;
        subscribeMessage_.end_ = end;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }

    SubscriptionBuilder& set_data_range(Filter::LatestPerGroupInTrack f)
    {
        subscribeMessage_.filterType_ = f.filterType;
        setElementsCounter_ |= (1 << utils::to_underlying(SetElements::Range));
        return *this;
    }

    SubscribeMessage build()
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
