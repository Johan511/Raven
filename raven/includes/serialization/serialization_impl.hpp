#pragma once

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
serialize_return_t serialize(ds::chunk& c, ds::quic_var_int i)
{
    static_assert(std::is_same_v<T, ds::quic_var_int>);
    std::uint64_t value = i.get();

    if (value < (1 << 6))
    {
        std::uint8_t value = i.get(); // 00xxxxxx
        return serialize_trivial<std::uint8_t>(c, value);
    }
    else if (value < (1 << 14))
    {
        // 01xxxxxx xxxxxxxx
        std::uint16_t value = (std::uint64_t(0b01) << 14) | i.get();
        return serialize_trivial<std::uint16_t>(c, value);
    }
    else if (value < (1 << 30))
    {
        // 10xxxxxx xxxxxxxx ...
        std::uint32_t value = (std::uint64_t(0b10) << 30) | i.get();
        return serialize_trivial<std::uint32_t>(c, value);
    }
    else
    {
        // 11xxxxxx xxxxxxxx ...
        std::uint64_t value = (std::uint64_t(0b11) << 62) | i.get();
        return serialize_trivial<std::uint64_t>(c, value);
    }
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
serialize_return_t serialize(ds::chunk& c, auto i)
{
    return serialize_trivial<T>(c, i);
}

template <UnsignedInteger T> serialize_return_t mock_serialize(const auto&)
{
    return sizeof(T);
}

///////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename ToEndianess = NetworkEndian>
serialize_return_t serialize_optional(ds::chunk& c, const std::optional<T>& i)
{
    if (i.has_value())
        return serialize<T>(c, i.get());
    else
        return 0;
}

template <typename T>
serialize_return_t mock_serialize_optional(const std::optional<T>& i)
{
    if (i.has_value())
        return mock_serialize<T>(i.get());
    else
        return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Message serialization
serialize_return_t
serialize(ds::chunk& c, const rvn::ClientSetupMessage& clientSetupMessage);
serialize_return_t
serialize(ds::chunk& c, const rvn::ServerSetupMessage& serverSetupMessage);
serialize_return_t serialize(ds::chunk& c, const rvn::SubscribeMessage& subscribeMessage);

serialize_return_t serialize(ds::chunk& c, const StreamHeaderSubgroupMessage& msg);

serialize_return_t serialize(ds::chunk& c, const StreamHeaderSubgroupObject& msg);
serialize_return_t
serialize(ds::chunk& c, const rvn::SubscribeErrorMessage& subscribeErrorMessage);

} // namespace rvn::serialization::detail
