#pragma once

extern "C"
{
#include <msquic.h>
}

///////////////////////////////////c
#include <cassert>
#include <cstdint>
#include <serialization/chunk.hpp>
#include <serialization/quic_var_int.hpp>
#include <serialization/serialization_impl.hpp>
#include <type_traits>
#include <utilities.hpp>
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
    if (std::is_constant_evaluated())
        return 8;
    else
        return i.size();
}

// (..) form
template <typename T> guess_size_t constexpr guess_size(const std::vector<T>& v)
{
    // we take a rough guess that generally there are 2 fields in the vector
    if (std::is_constant_evaluated())
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
    if (std::is_constant_evaluated())
        return 24 + 8;
    else
        return s.size() + guess_size(rvn::ds::quic_var_int(s.size()));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename... Args> QUIC_BUFFER* serialize(Args&&... args)
{
    ds::chunk c;
    (detail::serialize(c, args), ...);

    QUIC_BUFFER* totalQuicBuffer =
    static_cast<QUIC_BUFFER*>(malloc(sizeof(QUIC_BUFFER) + c.size()));

    totalQuicBuffer->Length = c.size();
    totalQuicBuffer->Buffer =
    reinterpret_cast<uint8_t*>(totalQuicBuffer) + sizeof(QUIC_BUFFER);

    memcpy(totalQuicBuffer->Buffer, c.data(), c.size());

    return totalQuicBuffer;
}
} // namespace rvn::serialization
