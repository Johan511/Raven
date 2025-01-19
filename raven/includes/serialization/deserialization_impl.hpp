#pragma once

#include <serialization/chunk.hpp>
#include <serialization/endianness.hpp>
#include <serialization/quic_var.hpp>

namespace rvn::serialization::detail
{
///////////////////////////////////////////////////////////////////////////////////////////////
// Deserialize trivial types (std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t)

// Num bytes consumed while deserializing
using deserialize_return_t = std::uint64_t;

template <typename T, Endianness FromEndianness = NetworkEndian>
deserialize_return_t
deserialize_trivial(auto& t, const ds::ChunkSpan& c, FromEndianness = network_endian)
{
    auto* beginPtr = c.data();
    if constexpr (std::is_same_v<FromEndianness, NativeEndian> || sizeof(T) == 1)
        t = *reinterpret_cast<T*>(beginPtr);
    else if constexpr (std::is_same_v<FromEndianness, BigEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            t = be16toh(*reinterpret_cast<std::uint16_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            t = be32toh(*reinterpret_cast<std::uint32_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint64_t>)
            t = be64toh(*reinterpret_cast<std::uint64_t*>(beginPtr));
        else
            static_assert(false, "Unsupported type, only 16, 32, 64 bit "
                                 "unsigned integers supported");
    }
    else if constexpr (std::is_same_v<FromEndianness, LittleEndian>)
    {
        if constexpr (std::is_same_v<T, std::uint16_t>)
            t = le16toh(*reinterpret_cast<std::uint16_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint32_t>)
            t = le32toh(*reinterpret_cast<std::uint32_t*>(beginPtr));
        else if constexpr (std::is_same_v<T, std::uint64_t>)
            t = le64toh(*reinterpret_cast<std::uint64_t*>(beginPtr));
        else
            static_assert(false, "Unsupported type, only 16, 32, 64 bit "
                                 "unsigned integers supported");
    }
    else
        static_assert(false, "Unsupported endianness");

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
deserialize(ds::quic_var_int& i, const ds::ChunkSpan& chunk, NetworkEndian = network_endian)
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
} // namespace rvn::serialization::detail
