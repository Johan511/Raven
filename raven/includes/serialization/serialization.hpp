#pragma once

///////////////////////////////////c
#include <exceptions.hpp>
#include <msquic.h>
#include <protobuf_messages.hpp>
#include <serialization/chunk.hpp>
#include <serialization/quic_var.hpp>
#include <utilities.hpp>
///////////////////////////////////
#include <google/protobuf/util/delimited_message_util.h>
#include <setup_messages.pb.h>
///////////////////////////////////

namespace rvn::serialization
{

/*
x (L):
Indicates that x is L bits long

x (i):
Indicates that x holds an integer value using the variable-length encoding as described in ([RFC9000], Section 16)

x (..):
Indicates that x can be any length including zero bits long. Values in this format always end on a byte boundary.

[x (L)]:
Indicates that x is optional and has a length of L

x (L) ...:
Indicates that x is repeated zero or more times and that each instance has a length of L

This document extends the RFC9000 syntax and with the additional field types:

x (b):
Indicates that x consists of a variable length integer encoding as described in ([RFC9000], Section 16), followed by that many bytes of binary data

x (tuple):
Indicates that x is a tuple, consisting of a variable length integer encoded as
described in ([RFC9000], Section 16), followed by that many variable length tuple fields, each of which are encoded as (b) above.
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
using guess_size_t = std::uint32_t;

template <typename T>
concept TriviallyConstructible = std::is_trivially_constructible_v<T>;

/*
x (L):
Indicates that x is L bits long
*/
template <TriviallyConstructible T> constexpr guess_size_t guess_size(const T&)
{
    return sizeof(T);
}

template <std::size_t N, TriviallyConstructible T>
constexpr guess_size_t guess_size(const std::array<T, N>&)
{
    return N * guess_size(std::declval<T>());
}

/*
x (i):
Indicates that x holds an integer value using the variable-length encoding as described in ([RFC9000], Section 16)
*/
// guesses 8 as possible size
constexpr guess_size_t guess_size(const rvn::ds::quic_var_int& i)
{
    if constexpr (std::is_constant_evaluated())
        return 8;
    else
        return i.size();
}

// (..) form
template <typename T> guess_size_t constexpr guess_size(const std::vector<T>& v)
{
    // we take a rough guess that generally there are 2 fields in the vector
    if constexpr (std::is_constant_evaluated())
        return guess_size(2 * std::declval<T>());
    else
        return guess_size(v.size() * std::declval<T>());
}

// [ ] form
template <typename T> guess_size_t constexpr guess_size(const std::optional<T>&)
{
    return guess_size(std::declval<T>());
}

/*
x (b):
Indicates that x consists of a variable length integer encoding as described in
([RFC9000], Section 16), followed by that many bytes of binary data
*/
constexpr guess_size_t guess_size(const std::string& s)
{
    // We take a rough guess that generally the size of the string is 24 bytes
    // and the size of the variable length integer encoding is 8 bytes
    if constexpr (std::is_constant_evaluated())
        return 24 + 8;
    else
        return s.size() + guess_size(rvn::ds::quic_var_int(s.size()));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace protobuf_messages;


namespace detail
{
    /*
        2 byte integer => xy
        y is lower byte
        x is higher byte

        Little Endian =>



    */
    struct LittleEndian
    {
    };
    struct BigEndian
    {
    };

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SYSTEM_LITTLE_ENDIAN 1
#else
#define SYSTEM_LITTLE_ENDIAN 0
#endif

    template <typename T>
    ds::chunk serialize_trivial(const T& t, const LittleEndian&)
    {
        static_assert(sizeof(T) > 0);
        ds::chunk c(sizeof(T));
#if SYSTEM_LITTLE_ENDIAN
        std::memcpy(c.data(), &t, sizeof(T));
#else
        // reverse the bytes
        constexpr std::size_t size = sizeof(T);
        char* p = reinterpret_cast<char*>(&t);
        for (std::size_t i = size - 1; i >= 0; --i)
            c.data()[i] = p[i];

#endif
        return c;
    }


    template <typename T>
    ds::chunk serialize_trivial(const T& t, const BigEndian&)
    {
        static_assert(sizeof(T) > 0);
        ds::chunk c(sizeof(T));
#if SYSTEM_LITTLE_ENDIAN
        // reverse the bytes
        constexpr std::size_t size = sizeof(T);
        char* p = reinterpret_cast<char*>(&t);
        for (std::size_t i = size - 1; i >= 0; --i)
            c.data()[i] = p[i];
#else
        std::memcpy(c.data(), &t, sizeof(T));
#endif
        return c;
    }

    template <typename Endianess>
    ds::chunk serialize(ds::quic_var_int i, const Endianess& e)
    {
        const auto chunkSize = i.size();

        switch (chunkSize)
        {
            case 1:
            {
                std::uint8_t value = i.value(); // 00xxxxxx
                utils::ASSERT_LOG_THROW(value == i.value(),
                                        "Value is too large for 1 byte");
                return serialize_trivial(value, e);
            }
            case 2:
            {
                std::uint16_t value = (0x40ULL << 8) | i.value(); // 01xxxxxx xxxxxxxx
                utils::ASSERT_LOG_THROW(value == i.value(),
                                        "Value is too large for 2 byte");
                return serialize_trivial(value, e);
            }
            case 4:
            {
                std::uint8_t value = (0x80ULL << 24) | i.value(); // 10xxxxxx xxxxxxxx ...
                utils::ASSERT_LOG_THROW(value == i.value(),
                                        "Value is too large for 4 byte");
                return serialize_trivial(value, e);
            }
            case 8:
            {
                std::uint8_t value = (0xC0ULL << 56) | i.value(); // 10xxxxxx xxxxxxxx ...
                utils::ASSERT_LOG_THROW(value == i.value(),
                                        "Value is too large for 8 byte");
                return serialize_trivial(value, e);
            }
        }

        assert(false);
    }
} // namespace detail


template <typename... Args> QUIC_BUFFER* serialize(Args&&... args)
{
    std::size_t requiredBufferSize = 0;
    std::ostringstream oss;
    (google::protobuf::util::SerializeDelimitedToOstream(args, &oss), ...);

    static constexpr std::uint32_t bufferCount = 1;
    std::string buffer = std::move(oss).str();

    void* sendBufferRaw = malloc(sizeof(QUIC_BUFFER) + buffer.size());
    utils::ASSERT_LOG_THROW(sendBufferRaw != nullptr,
                            "Could not allocate memory for buffer");


    QUIC_BUFFER* sendBuffer = (QUIC_BUFFER*)sendBufferRaw;
    sendBuffer->Buffer = (uint8_t*)sendBufferRaw + sizeof(QUIC_BUFFER);
    sendBuffer->Length = buffer.size();

    std::memcpy(sendBuffer->Buffer, buffer.c_str(), buffer.size());

    return sendBuffer;
}

template <typename T, typename InputStream> T deserialize(InputStream& istream)
{
    T t;
    bool clean_eof;
    google::protobuf::util::ParseDelimitedFromZeroCopyStream(&t, &istream, &clean_eof);
    if (clean_eof)
        throw rvn::exception::parsing_exception();
    return t;
};
} // namespace rvn::serialization
