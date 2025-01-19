#pragma once

#include <optional>
#include <serialization/chunk.hpp>
#include <serialization/endianness.hpp>
#include <serialization/quic_var.hpp>

namespace rvn::serialization::detail
{
///////////////////////////////////////////////////////////////////////////////////////////////
// Serialize and deserialize trivial types (std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t)

// By default we always want to serialize in network byte order
template <typename T, Endianness ToEndianness = NetworkEndian>
void serialize_trivial(ds::chunk& c, const auto& value, ToEndianness = network_endian)
{
    auto requiredSize = c.size() + sizeof(T);
    c.reserve(requiredSize);

    T valueCastToT = value;
    T valueModifiedEndianness;

    if constexpr (std::is_same_v<ToEndianness, NativeEndian> || std::is_same_v<T, std::uint8_t>)
        return c.append(&valueCastToT, sizeof(T));
    else if constexpr (std::is_same_v<ToEndianness, BigEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            valueModifiedEndianness = htobe16(valueCastToT);
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            valueModifiedEndianness = htobe32(valueCastToT);
        else if constexpr (std::is_same_v<T, std::uint64_t>)
            valueModifiedEndianness = htobe64(valueCastToT);
        else
            static_assert(false, "Unsupported type, only 16, 32, 64 bit "
                                 "unsigned integers supported");
    }
    else if constexpr (std::is_same_v<ToEndianness, LittleEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            valueModifiedEndianness = htole16(valueCastToT);
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            valueModifiedEndianness = htole32(valueCastToT);
        else if constexpr (std::is_same_v<T, std::uint64_t>)
            valueModifiedEndianness = htole64(valueCastToT);
        else
            static_assert(false, "Unsupported type, only 16, 32, 64 bit "
                                 "unsigned integers supported");
    }
    else
        static_assert(false, "Unsupported endianness");

    c.append(&valueModifiedEndianness, sizeof(T));
}

///////////////////////////////////////////////////////////////////////////////////////////////
/*
    x (i): -> quic_var_int
    Indicates that x holds an integer value using the variable-length encoding as described in ([RFC9000], Section 16)
*/
template <typename T, typename ToEndianess = NetworkEndian>
void serialize(ds::chunk& c, ds::quic_var_int i, ToEndianess = network_endian)
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
            // 10xxxxxx xxxxxxxx ...
            std::uint64_t value = (std::uint64_t(0b11) << 62) | i.value();
            return serialize_trivial<std::uint64_t>(c, value, ToEndianess{});
        }
    }
    assert(false);
}

/*
    x (L): -> std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
    Indicates that x is L bits long
*/
template <typename T, typename ToEndianess = NetworkEndian>
void serialize(ds::chunk& c, std::uint8_t i, ToEndianess = network_endian)
{
    static_assert(std::is_same_v<T, std::uint8_t>);
    return serialize_trivial<std::uint8_t>(c, i, ToEndianess{});
}
template <typename T, typename ToEndianess = NetworkEndian>
void serialize(ds::chunk& c, std::uint16_t i, ToEndianess = network_endian)
{
    static_assert(std::is_same_v<T, std::uint16_t>);
    return serialize_trivial<std::uint16_t>(c, i, ToEndianess{});
}
template <typename T, typename ToEndianess = NetworkEndian>
void serialize(ds::chunk& c, std::uint32_t i, ToEndianess = network_endian)
{
    static_assert(std::is_same_v<T, std::uint32_t>);
    return serialize_trivial<std::uint32_t>(c, i, ToEndianess{});
}
template <typename T, typename ToEndianess = NetworkEndian>
void serialize(ds::chunk& c, std::uint64_t i, ToEndianess = network_endian)
{
    static_assert(std::is_same_v<T, std::uint64_t>);
    return serialize_trivial<std::uint64_t>(c, i, ToEndianess{});
}

///////////////////////////////////////////////////////////////////////////////////////////////
/*
    If function template foo calls function template bar,
    bar should be defined before foo
    ```
        template <typename T, Endianness ToEndianness = NetworkEndian>
        ds::chunk serialize_trivial(const auto& t, ToEndianness = network_endian)
        {
            static_assert(sizeof(T) > 0);
            ds::chunk c(sizeof(T));
            serialize_trivial<T>(c, t, ToEndianness{});
            return c;
        }
    ```
    `serialize_trivial<T>(c, t, ToEndianness{});` must be defined before this

    This is because of some mess regarding the way C++ handles function template specialization
    https://cppquiz.org/quiz/question/251?result=OK&answer=3&did_answer=Answer
*/

template <typename T, typename ToEndianess = NetworkEndian>
void serialize(ds::chunk& c, const std::optional<T>& i, ToEndianess = network_endian)
{
    if (i.has_value())
        serialize<T>(c, i.value(), ToEndianess{});
}

template <typename T, typename ToEndianess = NetworkEndian>
ds::chunk serialize(auto& i, ToEndianess = network_endian)
{
    ds::chunk c;
    serialize<T>(c, i, ToEndianess{});
    return c;
}

template <typename T, Endianness ToEndianness = NetworkEndian>
ds::chunk serialize_trivial(const auto& t, ToEndianness = network_endian)
{
    static_assert(sizeof(T) > 0);
    ds::chunk c(sizeof(T));
    serialize_trivial<T>(c, t, ToEndianness{});

    return c;
}
///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace rvn::serialization::detail
