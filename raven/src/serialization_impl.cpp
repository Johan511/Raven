#include "serialization/chunk.hpp"
#include "serialization/messages.hpp"
#include "serialization/quic_var_int.hpp"
#include <serialization/serialization_impl.hpp>
#include <utilities.hpp>

namespace rvn::serialization::detail
{
///////////////////////////////////////////////////////////////////////////////////////////////
// Parameter serialization

[[nodiscard]] serialize_return_t
mock_serialize(const rvn::DeliveryTimeoutParameter& parameter)
{
    std::uint64_t parameterTotalLen = 0;
    parameterTotalLen += mock_serialize<ds::quic_var_int>(
    utils::to_underlying(ParameterType::DeliveryTimeout));
    std::uint64_t parameterLength = ds::quic_var_int(parameter.timeout_.count()).size();
    parameterTotalLen += mock_serialize<ds::quic_var_int>(parameterLength);
    parameterTotalLen += mock_serialize<ds::quic_var_int>(parameter.timeout_.count());
    return parameterTotalLen;
}

[[nodiscard]] serialize_return_t mock_serialize(const rvn::Parameter& parameter)
{
    return std::visit([](const auto& param) { return mock_serialize(param); },
                      parameter.parameter_);
}

serialize_return_t serialize(ds::chunk& c, const rvn::DeliveryTimeoutParameter& parameter)
{
    std::uint64_t parameterTotalLen = 0;
    parameterTotalLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(ParameterType::DeliveryTimeout));
    std::uint64_t parameterLength = ds::quic_var_int(parameter.timeout_.count()).size();
    parameterTotalLen += serialize<ds::quic_var_int>(c, parameterLength);
    parameterTotalLen += serialize<ds::quic_var_int>(c, parameter.timeout_.count());
    return parameterTotalLen;
}
serialize_return_t serialize(ds::chunk& c, const rvn::Parameter& parameter)
{
    return std::visit([&c](const auto& param) { return serialize(c, param); },
                      parameter.parameter_);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Message serialization
serialize_return_t serialize(ds::chunk& c, const rvn::ClientSetupMessage& clientSetupMessage)
{
    std::uint64_t msgLen = 0;
    // we need to find out length of the message we would be serializing
    {
        msgLen +=
        mock_serialize<ds::quic_var_int>(clientSetupMessage.supportedVersions_.size());
        for (const auto& version : clientSetupMessage.supportedVersions_)
            msgLen += mock_serialize<ds::quic_var_int>(version);

        msgLen +=
        mock_serialize<ds::quic_var_int>(clientSetupMessage.parameters_.size());
        for (const auto& parameter : clientSetupMessage.parameters_)
            msgLen += mock_serialize(parameter);
    }

    std::uint64_t headerLen = 0;

    // Header
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::CLIENT_SETUP));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    // Body
    serialize<ds::quic_var_int>(c, clientSetupMessage.supportedVersions_.size());
    for (const auto& version : clientSetupMessage.supportedVersions_)
        serialize<ds::quic_var_int>(c, version);

    serialize<ds::quic_var_int>(c, clientSetupMessage.parameters_.size());
    for (const auto& parameter : clientSetupMessage.parameters_)
        serialize(c, parameter);

    return headerLen + msgLen;
}

serialize_return_t serialize(ds::chunk& c, const rvn::ServerSetupMessage& serverSetupMessage)
{
    std::uint64_t msgLen = 0;
    // we need to find out length of the message we would be serializing
    {
        msgLen += mock_serialize<ds::quic_var_int>(serverSetupMessage.selectedVersion_);
        msgLen +=
        mock_serialize<ds::quic_var_int>(serverSetupMessage.parameters_.size());
        for (const auto& parameter : serverSetupMessage.parameters_)
            msgLen += mock_serialize(parameter);
    }

    std::uint64_t headerLen = 0;
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SERVER_SETUP));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    serialize<ds::quic_var_int>(c, serverSetupMessage.selectedVersion_);
    serialize<ds::quic_var_int>(c, serverSetupMessage.parameters_.size());
    for (const auto& parameter : serverSetupMessage.parameters_)
        serialize(c, parameter);

    return headerLen + msgLen;
}

static serialize_return_t mock_serialize(const rvn::SubscribeMessage& subscribeMessage)
{
    std::uint64_t msgLen = 0;
    msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.subscribeId_);
    msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.trackAlias_.get());

    msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.trackNamespace_.size());
    for (const auto& ns : subscribeMessage.trackNamespace_)
    {
        msgLen += mock_serialize<ds::quic_var_int>(ns.size());
        msgLen += ns.size();
    }

    msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.trackName_.size());
    msgLen += subscribeMessage.trackName_.size();

    msgLen += mock_serialize<std::uint8_t>(subscribeMessage.subscriberPriority_);
    msgLen += mock_serialize<std::uint8_t>(subscribeMessage.groupOrder_);
    msgLen +=
    mock_serialize<ds::quic_var_int>(utils::to_underlying(subscribeMessage.filterType_));

    if (subscribeMessage.start_.has_value())
    {
        msgLen +=
        mock_serialize<ds::quic_var_int>(subscribeMessage.start_->group_.get());
        msgLen +=
        mock_serialize<ds::quic_var_int>(subscribeMessage.start_->object_.get());
    }

    if (subscribeMessage.end_.has_value())
    {
        msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.end_->group_.get());
        msgLen +=
        mock_serialize<ds::quic_var_int>(subscribeMessage.end_->object_.get());
    }

    msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.parameters_.size());
    for (const auto& parameter : subscribeMessage.parameters_)
        msgLen += mock_serialize(parameter);

    return msgLen;
}

void serialize_without_header(ds::chunk& c, const rvn::SubscribeMessage& subscribeMessage)
{
    serialize<ds::quic_var_int>(c, subscribeMessage.subscribeId_);
    serialize<ds::quic_var_int>(c, subscribeMessage.trackAlias_.get());

    serialize<ds::quic_var_int>(c, subscribeMessage.trackNamespace_.size());
    for (const auto& ns : subscribeMessage.trackNamespace_)
    {
        serialize<ds::quic_var_int>(c, ns.size());
        c.append(ns.data(), ns.size());
    }

    serialize<ds::quic_var_int>(c, subscribeMessage.trackName_.size());
    c.append(subscribeMessage.trackName_.data(), subscribeMessage.trackName_.size());

    serialize<std::uint8_t>(c, subscribeMessage.subscriberPriority_);
    serialize<std::uint8_t>(c, subscribeMessage.groupOrder_);
    serialize<ds::quic_var_int>(c, utils::to_underlying(subscribeMessage.filterType_));

    if (subscribeMessage.start_.has_value())
    {
        serialize<ds::quic_var_int>(c, subscribeMessage.start_->group_.get());
        serialize<ds::quic_var_int>(c, subscribeMessage.start_->object_.get());
    }

    if (subscribeMessage.end_.has_value())
    {
        serialize<ds::quic_var_int>(c, subscribeMessage.end_->group_.get());
        serialize<ds::quic_var_int>(c, subscribeMessage.end_->object_.get());
    }

    serialize<ds::quic_var_int>(c, subscribeMessage.parameters_.size());
    for (const auto& parameter : subscribeMessage.parameters_)
        serialize(c, parameter);
}

serialize_return_t serialize(ds::chunk& c, const rvn::SubscribeMessage& subscribeMessage)
{
    std::uint64_t msgLen = mock_serialize(subscribeMessage);

    // header
    std::uint64_t headerLen = 0;
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SUBSCRIBE));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    // body
    serialize_without_header(c, subscribeMessage);

    return headerLen + msgLen;
}

serialize_return_t serialize(ds::chunk& c, const StreamHeaderSubgroupMessage& msg)
{
    std::uint64_t msgLen = 0;

    // header
    msgLen += serialize<ds::quic_var_int>(c, utils::to_underlying(msg.id_));

    // body
    msgLen += serialize<ds::quic_var_int>(c, msg.trackAlias_.get());
    msgLen += serialize<ds::quic_var_int>(c, msg.groupId_.get());
    msgLen += serialize<ds::quic_var_int>(c, msg.subgroupId_.get());
    msgLen += serialize<std::uint8_t>(c, msg.publisherPriority_);

    return msgLen;
}

serialize_return_t serialize(ds::chunk& c, const StreamHeaderSubgroupObject& msg)
{
    std::uint64_t msgLen = 0;
    // we need to find out length of the message we would be serializing
    {
        msgLen += mock_serialize<ds::quic_var_int>(msg.objectId_);
        msgLen += mock_serialize<ds::quic_var_int>(msg.payload_.size());
        msgLen += msg.payload_.size();
    }

    // no header for object messages

    // body
    serialize<ds::quic_var_int>(c, msg.objectId_);
    serialize<ds::quic_var_int>(c, msg.payload_.size());
    c.append(msg.payload_.data(), msg.payload_.size());

    return msgLen;
}

serialize_return_t
serialize(ds::chunk& c, const rvn::SubscribeErrorMessage& subscribeErrorMessage)
{
    std::uint64_t msgLen = 0;
    // Calculate the length of the message
    {
        msgLen += mock_serialize<ds::quic_var_int>(subscribeErrorMessage.subscribeId_);
        msgLen += mock_serialize<ds::quic_var_int>(subscribeErrorMessage.errorCode_);
        msgLen +=
        mock_serialize<ds::quic_var_int>(subscribeErrorMessage.reasonPhrase_.size());
        msgLen += subscribeErrorMessage.reasonPhrase_.size();
        msgLen += mock_serialize<ds::quic_var_int>(subscribeErrorMessage.trackAlias_);
    }

    std::uint64_t headerLen = 0;
    // Header
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SUBSCRIBE_ERROR));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    // Body
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.subscribeId_);
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.errorCode_);
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.reasonPhrase_.size());
    c.append(subscribeErrorMessage.reasonPhrase_.data(),
             subscribeErrorMessage.reasonPhrase_.size());
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.trackAlias_);

    return headerLen + msgLen;
}

serialize_return_t
serialize(ds::chunk& c, const rvn::BatchSubscribeMessage& batchSubscribeMessage)
{
    std::uint64_t msgLen = 0;
    // we need to find out length of the message we would be serializing
    {
        msgLen += mock_serialize<ds::quic_var_int>(
        batchSubscribeMessage.trackNamespacePrefix_.size());
        for (const auto& ns : batchSubscribeMessage.trackNamespacePrefix_)
        {
            msgLen += mock_serialize<ds::quic_var_int>(ns.size());
            msgLen += ns.size();
        }
        msgLen +=
        mock_serialize<ds::quic_var_int>(batchSubscribeMessage.subscriptions_.size());
        for (const auto& subscription : batchSubscribeMessage.subscriptions_)
            msgLen += mock_serialize(subscription);
    }

    std::uint64_t headerLen = 0;
    // Header
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::BATCH_SUBSCRIBE));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    serialize<ds::quic_var_int>(c, batchSubscribeMessage.trackNamespacePrefix_.size());
    for (const auto& ns : batchSubscribeMessage.trackNamespacePrefix_)
    {
        serialize<ds::quic_var_int>(c, ns.size());
        c.append(ns.data(), ns.size());
    }

    // Body
    serialize<ds::quic_var_int>(c, batchSubscribeMessage.subscriptions_.size());
    for (const auto& subscription : batchSubscribeMessage.subscriptions_)
        serialize_without_header(c, subscription);

    return headerLen + msgLen;
}

serialize_return_t
serialize(ds::chunk& c, const rvn::SubscribeUpdateMessage& subscribeUpdateMessage)
{
    std::uint64_t msgLen = 0;
    {
        msgLen += mock_serialize<ds::quic_var_int>(subscribeUpdateMessage.requestId_);
        msgLen += mock_serialize<ds::quic_var_int>(subscribeUpdateMessage.startLocation_.group_.get());
        msgLen += mock_serialize<ds::quic_var_int>(subscribeUpdateMessage.startLocation_.object_.get());
        msgLen += mock_serialize<ds::quic_var_int>(subscribeUpdateMessage.endGroup_);
        msgLen += mock_serialize<ds::quic_var_int>(subscribeUpdateMessage.subscriberPriority_);
        msgLen += mock_serialize<std::uint8_t>(subscribeUpdateMessage.forward_);
        msgLen += mock_serialize<ds::quic_var_int>(subscribeUpdateMessage.parameters_.size());
        for (const auto& parameter : subscribeUpdateMessage.parameters_)
            msgLen += mock_serialize(parameter);
    }

    std::uint64_t headerLen = 0;
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SUBSCRIBE_UPDATE));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    serialize<ds::quic_var_int>(c, subscribeUpdateMessage.requestId_);
    serialize<ds::quic_var_int>(c, subscribeUpdateMessage.startLocation_.group_.get());
    serialize<ds::quic_var_int>(c, subscribeUpdateMessage.startLocation_.object_.get());
    serialize<ds::quic_var_int>(c, subscribeUpdateMessage.endGroup_);
    serialize<std::uint8_t>(c, subscribeUpdateMessage.subscriberPriority_);
    serialize<std::uint8_t>(c, subscribeUpdateMessage.forward_);
    serialize<ds::quic_var_int>(c, subscribeUpdateMessage.parameters_.size());
    for (const auto& parameter : subscribeUpdateMessage.parameters_)
        serialize(c, parameter);
    return headerLen + msgLen;
}
} // namespace rvn::serialization::detail
