#include <serialization/serialization_impl.hpp>
#include <utilities.hpp>

namespace rvn::serialization::detail
{
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
        {
            msgLen += mock_serialize<ds::quic_var_int>(
            static_cast<std::uint32_t>(parameter.parameterType_));
            msgLen += mock_serialize<ds::quic_var_int>(parameter.parameterValue_.size());
            msgLen += parameter.parameterValue_.size();
        }
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
    {

        serialize<ds::quic_var_int>(c, utils::to_underlying(parameter.parameterType_));
        serialize<ds::quic_var_int>(c, parameter.parameterValue_.size());
        c.append(parameter.parameterValue_.data(), parameter.parameterValue_.size());
    }

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
        {
            msgLen += mock_serialize<ds::quic_var_int>(
            static_cast<std::uint32_t>(parameter.parameterType_));
            msgLen += mock_serialize<ds::quic_var_int>(parameter.parameterValue_.size());
            msgLen += parameter.parameterValue_.size();
        }
    }

    std::uint64_t headerLen = 0;
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SERVER_SETUP));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    serialize<ds::quic_var_int>(c, serverSetupMessage.selectedVersion_);
    serialize<ds::quic_var_int>(c, serverSetupMessage.parameters_.size());
    for (const auto& parameter : serverSetupMessage.parameters_)
    {
        serialize<ds::quic_var_int>(c, utils::to_underlying(parameter.parameterType_));
        serialize<ds::quic_var_int>(c, parameter.parameterValue_.size());
        c.append(parameter.parameterValue_.data(), parameter.parameterValue_.size());
    }

    return headerLen + msgLen;
}

serialize_return_t serialize(ds::chunk& c, const rvn::SubscribeMessage& subscribeMessage)
{
    std::uint64_t msgLen = 0;
    // we need to find out length of the message we would be serializing
    {
        msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.subscribeId_);
        msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.trackAlias_.get());

        msgLen +=
        mock_serialize<ds::quic_var_int>(subscribeMessage.trackNamespace_.size());
        for (const auto& ns : subscribeMessage.trackNamespace_)
        {
            msgLen += mock_serialize<ds::quic_var_int>(ns.size());
            msgLen += ns.size();
        }

        msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.trackName_.size());
        msgLen += subscribeMessage.trackName_.size();

        msgLen += mock_serialize<std::uint8_t>(subscribeMessage.subscriberPriority_);
        msgLen += mock_serialize<std::uint8_t>(subscribeMessage.groupOrder_);
        msgLen += mock_serialize<ds::quic_var_int>(
        utils::to_underlying(subscribeMessage.filterType_));

        if (subscribeMessage.start_.has_value())
        {
            msgLen +=
            mock_serialize<ds::quic_var_int>(subscribeMessage.start_->group_.get());
            msgLen +=
            mock_serialize<ds::quic_var_int>(subscribeMessage.start_->object_.get());
        }

        if (subscribeMessage.end_.has_value())
        {
            msgLen +=
            mock_serialize<ds::quic_var_int>(subscribeMessage.end_->group_.get());
            msgLen +=
            mock_serialize<ds::quic_var_int>(subscribeMessage.end_->object_.get());
        }

        msgLen += mock_serialize<ds::quic_var_int>(subscribeMessage.parameters_.size());
        for (const auto& parameter : subscribeMessage.parameters_)
        {
            msgLen += mock_serialize<ds::quic_var_int>(
            static_cast<std::uint32_t>(parameter.parameterType_));
            msgLen += mock_serialize<ds::quic_var_int>(parameter.parameterValue_.size());
            msgLen += parameter.parameterValue_.size();
        }
    }

    // header
    std::uint64_t headerLen = 0;
    headerLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SUBSCRIBE));
    headerLen += serialize<ds::quic_var_int>(c, msgLen);

    // body
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
    {
        serialize<ds::quic_var_int>(c, utils::to_underlying(parameter.parameterType_));
        serialize<ds::quic_var_int>(c, parameter.parameterValue_.size());
        c.append(parameter.parameterValue_.data(), parameter.parameterValue_.size());
    }

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
} // namespace rvn::serialization::detail