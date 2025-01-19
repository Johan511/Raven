#pragma once

#include <bit>

namespace rvn::serialization
{

// clang-format off
struct LittleEndian{};
struct BigEndian{};
// clang-format on

// TODO: deal with mixed endianness
static_assert(std::endian::native == std::endian::little ||
              std::endian::native == std::endian::big,
              "Mixed endianness not supported");

using NetworkEndian = BigEndian;
using NativeEndian =
std::conditional_t<std::endian::native == std::endian::little, LittleEndian, BigEndian>;

static constexpr LittleEndian little_endian{};
static constexpr BigEndian big_endian{};
static constexpr NetworkEndian network_endian{};
static constexpr NativeEndian native_endian{};

template <typename T>
concept Endianness = std::same_as<T, LittleEndian> || std::same_as<T, BigEndian>;

} // namespace rvn::serialization
