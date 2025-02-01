#pragma once

#include <serialization/chunk.hpp>
#include <serialization/endianness.hpp>
#include <serialization/messages.hpp>
#include <serialization/quic_var_int.hpp>
#include <string>

namespace rvn::serialization::detail
{
///////////////////////////////////////////////////////////////////////////////////////////////
// Deserialize trivial types (std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t)

// Num bytes consumed while deserializing
using deserialize_return_t = std::uint64_t;

template <typename T, typename ConstSpan, Endianness FromEndianness = NetworkEndian>
deserialize_return_t
deserialize_trivial(auto& t, ConstSpan& c, FromEndianness = network_endian)
{
    static_assert(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
                  std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);
    static_assert(std::is_same_v<FromEndianness, BigEndian> ||
                  std::is_same_v<FromEndianness, LittleEndian>);

    if constexpr (std::is_same_v<FromEndianness, NativeEndian> || sizeof(T) == 1)
        c.copy_to(&t, sizeof(T));
    else if constexpr (std::is_same_v<FromEndianness, BigEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
        {
            std::uint16_t spanEndianValue;
            c.copy_to(&spanEndianValue, sizeof(T));
            t = be16toh(spanEndianValue);
        }
        else if constexpr (std::is_same_v<T, std::uint32_t>)
        {
            std::uint32_t spanEndianValue;
            c.copy_to(&spanEndianValue, sizeof(T));
            t = be32toh(spanEndianValue);
        }
        else /* if constexpr (std::is_same_v<T, std::uint64_t>) */
        {
            std::uint64_t spanEndianValue;
            c.copy_to(&spanEndianValue, sizeof(T));
            t = be64toh(spanEndianValue);
        }
    }
    else /* if constexpr (std::is_same_v<FromEndianness, LittleEndian>) */
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
        {
            std::uint16_t spanEndianValue;
            c.copy_to(&spanEndianValue, sizeof(T));
            t = le16toh(spanEndianValue);
        }
        else if constexpr (std::is_same_v<T, std::uint32_t>)
        {
            std::uint32_t spanEndianValue;
            c.copy_to(&spanEndianValue, sizeof(T));
            t = le32toh(spanEndianValue);
        }
        else /* if constexpr (std::is_same_v<T, std::uint64_t>) */
        {
            std::uint64_t spanEndianValue;
            c.copy_to(&spanEndianValue, sizeof(T));
            t = le64toh(spanEndianValue);
        }
    }

    c.advance_begin(sizeof(T));
    return sizeof(T);
}
///////////////////////////////////////////////////////////////////////////////////////////////
// TODO: extern templates for faster preprocessing phase during compilation + lower object size
/*
    We can't deserialize a quic_var_int from little endian beacuse
    higher order bits are used to indicate the length of the integer
*/
template <typename T, typename ConstSpan, Endianness FromEndianness = NetworkEndian>
deserialize_return_t
deserialize(std::uint64_t& i, ConstSpan& chunk, NetworkEndian = network_endian)
{
    static_assert(std::is_same_v<T, ds::quic_var_int>);

    std::uint8_t prefix2Bits = chunk[0] >> 6;
    switch (prefix2Bits)
    {
        case 0b00:
        {
            // 1 byte integer
            using t00 = std::uint8_t;
            t00 prefixedValue;
            std::uint64_t numBytesDeserialized =
            deserialize_trivial<t00>(prefixedValue, chunk, NetworkEndian{});
            i = prefixedValue & ~(t00(0b11) << 6);
            return numBytesDeserialized;
        }
        case 0b01:
        {
            // 2 byte integer
            using t01 = std::uint16_t;
            t01 prefixedValue;
            std::uint64_t numBytesDeserialized =
            deserialize_trivial<t01>(prefixedValue, chunk, NetworkEndian{});
            i = prefixedValue & ~(t01(0b11) << 14);
            return numBytesDeserialized;
        }
        case 0b10:
        {
            // 4 byte integer
            using t10 = std::uint32_t;
            t10 prefixedValue;
            std::uint64_t numBytesDeserialized =
            deserialize_trivial<t10>(prefixedValue, chunk, NetworkEndian{});
            i = prefixedValue & ~(t10(0b11) << 30);
            return numBytesDeserialized;
        }
        case 0b11:
        {
            // 8 byte integer
            using t11 = std::uint64_t;
            t11 prefixedValue;
            std::uint64_t numBytesDeserialized =
            deserialize_trivial<t11>(prefixedValue, chunk, NetworkEndian{});
            i = prefixedValue & ~(t11(0b11) << 62);
            return numBytesDeserialized;
        }
    }

    assert(false);
    return 42;
}

template <typename ConstSpan>
static inline deserialize_return_t
deserialize_params(std::vector<depracated::messages::Parameter>& parameters,
                   ConstSpan& span,
                   NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    std::uint64_t numParameters;
    deserializedBytes += deserialize<ds::quic_var_int>(numParameters, span);
    parameters.resize(numParameters);
    for (auto& parameter : parameters)
    {
        std::uint64_t parameterType;
        deserializedBytes += deserialize<ds::quic_var_int>(parameterType, span);
        parameter.parameterType_ =
        static_cast<depracated::messages::ParameterType>(parameterType);

        std::uint64_t parameterLength;
        deserializedBytes += deserialize<ds::quic_var_int>(parameterLength, span);

        parameter.parameterValue_ = std::string(parameterLength, '\0');
        span.copy_to(parameter.parameterValue_.data(), parameterLength);
        span.advance_begin(parameterLength);
        deserializedBytes += parameterLength;
    }

    return deserializedBytes;
}
///////////////////////////////////////////////////////////////////////////////////
// Message Deserialization
// precondition: span is from start of message to end of message
template <typename ConstSpan>
static inline deserialize_return_t
deserialize(rvn::depracated::messages::ClientSetupMessage& clientSetupMessage,
            ConstSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    std::uint64_t numSupportedVersions;
    deserializedBytes += deserialize<ds::quic_var_int>(numSupportedVersions, span);
    clientSetupMessage.supportedVersions_.resize(numSupportedVersions);
    for (auto& version : clientSetupMessage.supportedVersions_)
    {
        std::uint64_t version64Bit;
        deserializedBytes += deserialize<ds::quic_var_int>(version64Bit, span);
        // We can safely cast to uint32 as version is 32 bit uint
        // https://www.ietf.org/archive/id/draft-ietf-moq-transport-07.html#section-6.2.1
        version = static_cast<std::uint32_t>(version64Bit);
    }

    deserializedBytes += deserialize_params(clientSetupMessage.parameters_, span);

    return deserializedBytes;
}

template <typename ConstSpan>
static inline deserialize_return_t
deserialize(rvn::depracated::messages::ControlMessageHeader& controlMessageHeader,
            ConstSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    std::uint64_t messageType;
    deserializedBytes += deserialize<ds::quic_var_int>(messageType, span);
    controlMessageHeader.messageType_ =
    static_cast<depracated::messages::MoQtMessageType>(messageType);

    std::uint64_t length;
    deserializedBytes += deserialize<ds::quic_var_int>(length, span);
    controlMessageHeader.length_ = length;

    return deserializedBytes;
}

template <typename ConstSpan>
static inline deserialize_return_t
deserialize(rvn::depracated::messages::ServerSetupMessage& serverSetupMessage,
            ConstSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    std::uint64_t selectedVersion;
    deserializedBytes += deserialize<ds::quic_var_int>(selectedVersion, span);
    serverSetupMessage.selectedVersion_ = selectedVersion;

    deserializedBytes += deserialize_params(serverSetupMessage.parameters_, span);

    return deserializedBytes;
}

template <typename ConstSpan>
static inline deserialize_return_t
deserialize(rvn::depracated::messages::SubscribeMessage& subscribeMessage,
            ConstSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeMessage.subscribeId_, span);
    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeMessage.trackAlias_.get(), span);

    std::uint64_t numTrackNamespace;
    deserializedBytes += deserialize<ds::quic_var_int>(numTrackNamespace, span);
    subscribeMessage.trackNamespace_.resize(numTrackNamespace);
    for (auto& ns : subscribeMessage.trackNamespace_)
    {
        std::uint64_t nsLength;
        deserializedBytes += deserialize<ds::quic_var_int>(nsLength, span);

        ns = std::string(nsLength, '\0');
        span.copy_to(ns.data(), nsLength);
        deserializedBytes += nsLength;
        span.advance_begin(nsLength);
    }

    std::uint64_t trackNameLength;
    deserializedBytes += deserialize<ds::quic_var_int>(trackNameLength, span);
    subscribeMessage.trackName_ = std::string(trackNameLength, '\0');
    span.copy_to(subscribeMessage.trackName_.data(), trackNameLength);
    deserializedBytes += trackNameLength;
    span.advance_begin(trackNameLength);

    deserializedBytes +=
    deserialize_trivial<std::uint8_t>(subscribeMessage.subscriberPriority_, span);
    deserializedBytes +=
    deserialize_trivial<std::uint8_t>(subscribeMessage.groupOrder_, span);

    std::uint64_t filterType;
    deserializedBytes += deserialize<ds::quic_var_int>(filterType, span);
    subscribeMessage.filterType_ =
    static_cast<depracated::messages::SubscribeMessage::FilterType>(filterType);
    subscribeMessage.filterType_ =
    static_cast<depracated::messages::SubscribeMessage::FilterType>(filterType);

    if ((subscribeMessage.filterType_ ==
         rvn::depracated::messages::SubscribeMessage::FilterType::AbsoluteStart) |
        (subscribeMessage.filterType_ ==
         rvn::depracated::messages::SubscribeMessage::FilterType::AbsoluteRange))
    {
        subscribeMessage.start_.emplace();
        deserializedBytes +=
        deserialize<ds::quic_var_int>(subscribeMessage.start_->group_.get(), span);
        deserializedBytes +=
        deserialize<ds::quic_var_int>(subscribeMessage.start_->object_.get(), span);
    }

    if (subscribeMessage.filterType_ ==
        rvn::depracated::messages::SubscribeMessage::FilterType::AbsoluteRange)
    {
        subscribeMessage.end_.emplace();
        deserializedBytes +=
        deserialize<ds::quic_var_int>(subscribeMessage.end_->group_.get(), span);
        deserializedBytes +=
        deserialize<ds::quic_var_int>(subscribeMessage.end_->object_.get(), span);
    }

    deserializedBytes += deserialize_params(subscribeMessage.parameters_, span);

    return deserializedBytes;
}

template <typename ConstSpan>
static inline deserialize_return_t
deserialize(rvn::depracated::messages::SubscribeUpdateMessage& subscribeUpdateMessage,
            ConstSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeUpdateMessage.subscribeId_, span);
    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeUpdateMessage.startGroup_, span);
    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeUpdateMessage.startObject_, span);
    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeUpdateMessage.endGroup_, span);
    deserializedBytes +=
    deserialize<ds::quic_var_int>(subscribeUpdateMessage.endObject_, span);
    deserializedBytes +=
    deserialize_trivial<std::uint8_t>(subscribeUpdateMessage.subscriberPriority_, span);
    deserializedBytes += deserialize_params(subscribeUpdateMessage.parameters_, span);

    return deserializedBytes;
}
} // namespace rvn::serialization::detail
