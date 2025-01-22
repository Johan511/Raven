#pragma once

///////////////////////////////////c
#include <cassert>
#include <cstdint>
#include <exceptions.hpp>
#include <msquic.h>
#include <protobuf_messages.hpp>
#include <serialization/chunk.hpp>
#include <serialization/quic_var_int.hpp>
#include <type_traits>
#include <utilities.hpp>
///////////////////////////////////
#include <google/protobuf/util/delimited_message_util.h>
#include <setup_messages.pb.h>
///////////////////////////////////

namespace rvn::serialization
{

/*
x (L): -> std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
Indicates that x is L bits long

x (i): -> quic_var_int
Indicates that x holds an integer value using the variable-length encoding as described in ([RFC9000], Section 16)

x (..): 
Indicates that x can be any length including zero bits long. Values in this format always end on a byte boundary.

[x (L)]: -> std::optional<quic_var_int>
Indicates that x is optional and has a length of L

x (L) ...: -> Container<T> -> serialize all of them without any number of elements tag
Indicates that x is repeated zero or more times and that each instance has a length of L

This document extends the RFC9000 syntax and with the additional field types:

x (b): -> std::string or ds::chunk, serialized as quic_var_int then memcpy (do we care about endianess?)
Indicates that x consists of a variable length integer encoding as described in ([RFC9000], Section 16), followed by that many bytes of binary data

x (tuple):
Indicates that x is a tuple, consisting of a variable length integer encoded as
described in ([RFC9000], Section 16), followed by that many variable length tuple fields, each of which are encoded as (b) above.
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
using guess_size_t = std::uint64_t;

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
