#pragma once

#include "utilities.hpp"
#include <optional>
#include <serialization/chunk.hpp>
#include <serialization/endianness.hpp>
#include <serialization/messages.hpp>
#include <serialization/quic_var_int.hpp>

namespace rvn::serialization::detail
{
///////////////////////////////////////////////////////////////////////////////////////////////
// Serialize and deserialize trivial types (std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t)

template <typename T>
concept UnsignedInteger =
std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>;

using serialize_return_t = std::uint64_t;

// By default we always want to serialize in network byte order
template <UnsignedInteger T, Endianness ToEndianness = NetworkEndian>
serialize_return_t
serialize_trivial(ds::chunk& c, const auto& value, ToEndianness = network_endian)
{
    static_assert(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
                  std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>,
                  "Unsupported type, only 8, 16, 32, 64 bit "
                  "unsigned integers supported");
    static_assert(std::is_same_v<ToEndianness, BigEndian> ||
                  std::is_same_v<ToEndianness, LittleEndian>,
                  "Unsupported endianness");

    auto requiredSize = c.size() + sizeof(T);
    c.reserve(requiredSize);

    T valueCastToT = value;
    T valueModifiedEndianness;

    if constexpr (std::is_same_v<ToEndianness, NativeEndian> || std::is_same_v<T, std::uint8_t>)
    {
        c.append(&valueCastToT, sizeof(T));
        return sizeof(T);
    }
    else if constexpr (std::is_same_v<ToEndianness, BigEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            valueModifiedEndianness = htobe16(valueCastToT);
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            valueModifiedEndianness = htobe32(valueCastToT);
        else /* if constexpr (std::is_same_v<T, std::uint64_t>) */
            valueModifiedEndianness = htobe64(valueCastToT);
    }
    else /* if constexpr (std::is_same_v<ToEndianness, LittleEndian>) */
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            valueModifiedEndianness = htole16(valueCastToT);
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            valueModifiedEndianness = htole32(valueCastToT);
        else /* if constexpr (std::is_same_v<T, std::uint64_t>) */
            valueModifiedEndianness = htole64(valueCastToT);
    }

    c.append(&valueModifiedEndianness, sizeof(T));

    return sizeof(T);
}

///////////////////////////////////////////////////////////////////////////////////////////////
/*
    x (i): -> quic_var_int
    Indicates that x holds an integer value using the variable-length encoding as described in ([RFC9000], Section 16)
*/
template <typename T, typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, ds::quic_var_int i, ToEndianess = network_endian)
{
    static_assert(std::is_same_v<T, ds::quic_var_int>);

    const auto chunkSize = i.size();
    switch (chunkSize)
    {
        case 1:
        {
            std::uint8_t value = i.value(); // 00xxxxxx
            return serialize_trivial<std::uint8_t>(c, value, ToEndianess{});
        }
        case 2:
        {
            // 01xxxxxx xxxxxxxx
            std::uint16_t value = (std::uint64_t(0b01) << 14) | i.value();
            return serialize_trivial<std::uint16_t>(c, value, ToEndianess{});
        }
        case 4:
        {
            // 10xxxxxx xxxxxxxx ...
            std::uint32_t value = (std::uint64_t(0b10) << 30) | i.value();
            return serialize_trivial<std::uint32_t>(c, value, ToEndianess{});
        }
        case 8:
        {
            // 11xxxxxx xxxxxxxx ...
            std::uint64_t value = (std::uint64_t(0b11) << 62) | i.value();
            return serialize_trivial<std::uint64_t>(c, value, ToEndianess{});
        }
    }
    assert(false);
    return 42;
}

template <typename T> serialize_return_t mock_serialize(ds::quic_var_int i)
{
    static_assert(std::is_same_v<T, ds::quic_var_int>);

    return i.size();
}

/*
    x (L): -> std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
    Indicates that x is L bits long
*/
template <UnsignedInteger T, typename ToEndianess = NetworkEndian>
serialize_return_t serialize(ds::chunk& c, auto i, ToEndianess = network_endian)
{
    return serialize_trivial<T>(c, i, ToEndianess{});
}

template <UnsignedInteger T> serialize_return_t mock_serialize(const auto&)
{
    return sizeof(T);
}

///////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename ToEndianess = NetworkEndian>
serialize_return_t
serialize_optional(ds::chunk& c, const std::optional<T>& i, ToEndianess = network_endian)
{
    if (i.has_value())
        return serialize<T>(c, i.value(), ToEndianess{});
    else
        return 0;
}

template <typename T>
serialize_return_t mock_serialize_optional(const std::optional<T>& i)
{
    if (i.has_value())
        return mock_serialize<T>(i.value());
    else
        return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Message serialization
template <typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, const rvn::ClientSetupMessage& clientSetupMessage, ToEndianess = network_endian)
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
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::CLIENT_SETUP),
                                ToEndianess{});
    headerLen += serialize<ds::quic_var_int>(c, msgLen, ToEndianess{});

    // Body
    serialize<ds::quic_var_int>(c, clientSetupMessage.supportedVersions_.size(),
                                ToEndianess{});
    for (const auto& version : clientSetupMessage.supportedVersions_)
        serialize<ds::quic_var_int>(c, version, ToEndianess{});

    serialize<ds::quic_var_int>(c, clientSetupMessage.parameters_.size(), ToEndianess{});
    for (const auto& parameter : clientSetupMessage.parameters_)
    {

        serialize<ds::quic_var_int>(c, utils::to_underlying(parameter.parameterType_),
                                    ToEndianess{});
        serialize<ds::quic_var_int>(c, parameter.parameterValue_.size(), ToEndianess{});
        c.append(parameter.parameterValue_.data(), parameter.parameterValue_.size());
    }

    return headerLen + msgLen;
}

template <typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, const rvn::ServerSetupMessage& serverSetupMessage, ToEndianess = network_endian)
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
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SERVER_SETUP),
                                ToEndianess{});
    headerLen += serialize<ds::quic_var_int>(c, msgLen, ToEndianess{});

    serialize<ds::quic_var_int>(c, serverSetupMessage.selectedVersion_, ToEndianess{});
    serialize<ds::quic_var_int>(c, serverSetupMessage.parameters_.size(), ToEndianess{});
    for (const auto& parameter : serverSetupMessage.parameters_)
    {
        serialize<ds::quic_var_int>(c, utils::to_underlying(parameter.parameterType_),
                                    ToEndianess{});
        serialize<ds::quic_var_int>(c, parameter.parameterValue_.size(), ToEndianess{});
        c.append(parameter.parameterValue_.data(), parameter.parameterValue_.size());
    }

    return headerLen + msgLen;
}

template <typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, const rvn::SubscribeMessage& subscribeMessage, ToEndianess = network_endian)
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
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SUBSCRIBE),
                                ToEndianess{});
    headerLen += serialize<ds::quic_var_int>(c, msgLen, ToEndianess{});

    // body
    serialize<ds::quic_var_int>(c, subscribeMessage.subscribeId_, ToEndianess{});
    serialize<ds::quic_var_int>(c, subscribeMessage.trackAlias_.get(), ToEndianess{});

    serialize<ds::quic_var_int>(c, subscribeMessage.trackNamespace_.size(), ToEndianess{});
    for (const auto& ns : subscribeMessage.trackNamespace_)
    {
        serialize<ds::quic_var_int>(c, ns.size(), ToEndianess{});
        c.append(ns.data(), ns.size());
    }

    serialize<ds::quic_var_int>(c, subscribeMessage.trackName_.size(), ToEndianess{});
    c.append(subscribeMessage.trackName_.data(), subscribeMessage.trackName_.size());

    serialize<std::uint8_t>(c, subscribeMessage.subscriberPriority_, ToEndianess{});
    serialize<std::uint8_t>(c, subscribeMessage.groupOrder_, ToEndianess{});
    serialize<ds::quic_var_int>(c, utils::to_underlying(subscribeMessage.filterType_),
                                ToEndianess{});

    if (subscribeMessage.start_.has_value())
    {
        serialize<ds::quic_var_int>(c, subscribeMessage.start_->group_.get(), ToEndianess{});
        serialize<ds::quic_var_int>(c, subscribeMessage.start_->object_.get(),
                                    ToEndianess{});
    }

    if (subscribeMessage.end_.has_value())
    {
        serialize<ds::quic_var_int>(c, subscribeMessage.end_->group_.get(), ToEndianess{});
        serialize<ds::quic_var_int>(c, subscribeMessage.end_->object_.get(), ToEndianess{});
    }

    serialize<ds::quic_var_int>(c, subscribeMessage.parameters_.size(), ToEndianess{});
    for (const auto& parameter : subscribeMessage.parameters_)
    {
        serialize<ds::quic_var_int>(c, utils::to_underlying(parameter.parameterType_),
                                    ToEndianess{});
        serialize<ds::quic_var_int>(c, parameter.parameterValue_.size(), ToEndianess{});
        c.append(parameter.parameterValue_.data(), parameter.parameterValue_.size());
    }

    return headerLen + msgLen;
}

template <typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, const StreamHeaderSubgroupMessage& msg, ToEndianess = network_endian)
{
    std::uint64_t msgLen = 0;

    // header
    msgLen +=
    serialize<ds::quic_var_int>(c, utils::to_underlying(msg.id_), ToEndianess{});

    // body
    msgLen += serialize<ds::quic_var_int>(c, msg.trackAlias_.get(), ToEndianess{});
    msgLen += serialize<ds::quic_var_int>(c, msg.groupId_.get(), ToEndianess{});
    msgLen += serialize<ds::quic_var_int>(c, msg.subgroupId_.get(), ToEndianess{});
    msgLen += serialize<std::uint8_t>(c, msg.publisherPriority_, ToEndianess{});

    return msgLen;
}

template <typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, const StreamHeaderSubgroupObject& msg, ToEndianess = network_endian)
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
    serialize<ds::quic_var_int>(c, msg.objectId_, ToEndianess{});
    serialize<ds::quic_var_int>(c, msg.payload_.size(), ToEndianess{});
    c.append(msg.payload_.data(), msg.payload_.size());

    return msgLen;
}

template <typename ToEndianess = NetworkEndian>
serialize_return_t
serialize(ds::chunk& c, const rvn::SubscribeErrorMessage& subscribeErrorMessage, ToEndianess = network_endian)
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
    serialize<ds::quic_var_int>(c, utils::to_underlying(MoQtMessageType::SUBSCRIBE_ERROR),
                                ToEndianess{});
    headerLen += serialize<ds::quic_var_int>(c, msgLen, ToEndianess{});

    // Body
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.subscribeId_, ToEndianess{});
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.errorCode_, ToEndianess{});
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.reasonPhrase_.size(),
                                ToEndianess{});
    c.append(subscribeErrorMessage.reasonPhrase_.data(),
             subscribeErrorMessage.reasonPhrase_.size());
    serialize<ds::quic_var_int>(c, subscribeErrorMessage.trackAlias_, ToEndianess{});

    return headerLen + msgLen;
}

} // namespace rvn::serialization::detail
