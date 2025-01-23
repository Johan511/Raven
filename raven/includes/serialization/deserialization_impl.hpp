#pragma once

#include <serialization/chunk.hpp>
#include <serialization/endianness.hpp>
#include <serialization/messages.hpp>
#include <serialization/quic_var_int.hpp>

namespace rvn::serialization::detail
{
///////////////////////////////////////////////////////////////////////////////////////////////
// Deserialize trivial types (std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t)

// Num bytes consumed while deserializing
using deserialize_return_t = std::uint64_t;

template <typename T, Endianness FromEndianness = NetworkEndian>
deserialize_return_t
deserialize_trivial(auto& t, ds::ChunkSpan& c, FromEndianness = network_endian)
{
    auto* beginPtr = c.data();
    if constexpr (std::is_same_v<FromEndianness, NativeEndian> || sizeof(T) == 1)
        t = *reinterpret_cast<const T*>(beginPtr);
    else if constexpr (std::is_same_v<FromEndianness, BigEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            t = be16toh(*reinterpret_cast<const std::uint16_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            t = be32toh(*reinterpret_cast<const std::uint32_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint64_t>)
            t = be64toh(*reinterpret_cast<const std::uint64_t*>(beginPtr));
        else
            static_assert(false, "Unsupported type, only 16, 32, 64 bit "
                                 "unsigned integers supported");
    }
    else if constexpr (std::is_same_v<FromEndianness, LittleEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            t = le16toh(*reinterpret_cast<const std::uint16_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            t = le32toh(*reinterpret_cast<const std::uint32_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint64_t>)
            t = le64toh(*reinterpret_cast<const std::uint64_t*>(beginPtr));
        else
            static_assert(false, "Unsupported type, only 16, 32, 64 bit "
                                 "unsigned integers supported");
    }
    else
        static_assert(false, "Unsupported endianness");

    c.advance_begin(sizeof(T));
    return sizeof(T);
}
///////////////////////////////////////////////////////////////////////////////////////////////
// TODO: extern templates for faster preprocessing phase during compilation + lower object size
/*
    We can't deserialize a quic_var_int from little endian beacuse
    higher order bits are used to indicate the length of the integer
*/
template <typename T>
deserialize_return_t
deserialize(std::uint64_t& i, ds::ChunkSpan& chunk, NetworkEndian = network_endian)
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
///////////////////////////////////////////////////////////////////////////////////
// Message Deserialization
// precondition: span is from start of message to end of message
static inline deserialize_return_t
deserialize(rvn::depracated::messages::ClientSetupMessage& clientSetupMessage,
            ds::ChunkSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    std::uint64_t numSupportedVersions;
    deserializedBytes += deserialize<ds::quic_var_int>(numSupportedVersions, span);
    clientSetupMessage.supportedVersions_.resize(numSupportedVersions);
    for (auto& version : clientSetupMessage.supportedVersions_)
    {
        std::uint64_t version64Bit;
        deserialize<ds::quic_var_int>(version64Bit, span);
        // We can safely cast to uint32 as version is 32 bit uint
        // https://www.ietf.org/archive/id/draft-ietf-moq-transport-07.html#section-6.2.1
        version = static_cast<std::uint32_t>(version64Bit);
    }

    std::uint64_t numParameters;
    deserializedBytes += deserialize<ds::quic_var_int>(numParameters, span);
    clientSetupMessage.parameters_.resize(numParameters);
    for (auto& parameter : clientSetupMessage.parameters_)
    {
        std::uint64_t parameterType;
        deserializedBytes += deserialize<ds::quic_var_int>(parameterType, span);
        parameter.parameterType_ =
        static_cast<depracated::messages::ParameterType>(parameterType);

        std::uint64_t parameterLength;
        deserializedBytes += deserialize<ds::quic_var_int>(parameterLength, span);
        parameter.parameterValue_.resize(parameterLength);

        span.copy_to(parameter.parameterValue_.data(), parameterLength);
        deserializedBytes += parameterLength;
    }

    return deserializedBytes;
}

static inline deserialize_return_t
deserialize(rvn::depracated::messages::ControlMessageHeader& controlMessageHeader,
            ds::ChunkSpan& span,
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

static inline deserialize_return_t
deserialize(rvn::depracated::messages::ServerSetupMessage& serverSetupMessage,
            ds::ChunkSpan& span,
            NetworkEndian = network_endian)
{
    std::uint64_t deserializedBytes = 0;

    std::uint64_t selectedVersion;
    deserializedBytes += deserialize<ds::quic_var_int>(selectedVersion, span);
    serverSetupMessage.selectedVersion_ = selectedVersion;

    std::uint64_t numParameters;
    deserializedBytes += deserialize<ds::quic_var_int>(numParameters, span);
    serverSetupMessage.parameters_.resize(numParameters);
    for (auto& parameter : serverSetupMessage.parameters_)
    {
        std::uint64_t parameterType;
        deserializedBytes += deserialize<ds::quic_var_int>(parameterType, span);
        parameter.parameterType_ =
        static_cast<depracated::messages::ParameterType>(parameterType);

        std::uint64_t parameterLength;
        deserializedBytes += deserialize<ds::quic_var_int>(parameterLength, span);
        parameter.parameterValue_.resize(parameterLength);

        span.copy_to(parameter.parameterValue_.data(), parameterLength);
        deserializedBytes += parameterLength;
    }

    return deserializedBytes;
}
} // namespace rvn::serialization::detail
