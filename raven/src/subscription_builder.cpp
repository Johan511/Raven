#include <subscription_builder.hpp>

namespace rvn
{

std::uint64_t SubscriptionBuilder::subscribeIDCounter = 0;

// latest group
template <>
SubscriptionBuilder&
SubscriptionBuilder::set_data_range<protobuf_messages::SubscribeFilter::LatestGroup>(std::uint64_t startGroup)
{
    subscribeMessage.set_filtertype(protobuf_messages::SubscribeFilter::LatestGroup);
    subscribeMessage.set_startgroup(startGroup);
    setElementsCounter |= (1 << utils::to_underlying(SetElements::Range));
    return *this;
}

// latest object
template <>
SubscriptionBuilder&
SubscriptionBuilder::set_data_range<protobuf_messages::SubscribeFilter::LatestObject>(std::uint64_t startGroup)
{
    subscribeMessage.set_filtertype(protobuf_messages::SubscribeFilter::LatestObject);
    subscribeMessage.set_startgroup(startGroup);
    setElementsCounter |= (1 << utils::to_underlying(SetElements::Range));
    return *this;
}

// absolute start
template <>
SubscriptionBuilder&
SubscriptionBuilder::set_data_range<protobuf_messages::SubscribeFilter::AbsoluteStart>(
std::uint64_t startGroup,
std::uint64_t startObject)
{
    subscribeMessage.set_filtertype(protobuf_messages::SubscribeFilter::AbsoluteStart);
    subscribeMessage.set_startgroup(startGroup);
    subscribeMessage.set_startobject(startObject);
    setElementsCounter |= (1 << utils::to_underlying(SetElements::Range));
    return *this;
}

// Absolute Range
template <>
SubscriptionBuilder&
SubscriptionBuilder::set_data_range<protobuf_messages::SubscribeFilter::AbsoluteRange>(
std::uint64_t startGroup,
std::uint64_t startObject,
std::uint64_t endGroup,
std::uint64_t endObject)
{
    subscribeMessage.set_filtertype(protobuf_messages::SubscribeFilter::AbsoluteRange);
    subscribeMessage.set_startgroup(startGroup);
    subscribeMessage.set_startobject(startObject);
    subscribeMessage.set_endgroup(endGroup);
    subscribeMessage.set_endobject(endObject);
    setElementsCounter |= (1 << utils::to_underlying(SetElements::Range));
    return *this;
}


} // namespace rvn