#pragma once

#include <cstdint>
#include <strong_types.hpp>

namespace rvn::ds {
/*
    // integer which is variable length encoded
    Length determination based on the top two bits of the first byte:
    00 => 1 byte total (6 bits of payload)
    01 => 2 bytes total (14 bits of payload)
    10 => 4 bytes total (30 bits of payload)
    11 => 8 bytes total (62 bits of payload)
*/
class quic_var_int : detail::UintCTRPTrait<std::uint64_t, quic_var_int> {
  std::uint64_t value_{};

public:
  constexpr quic_var_int(std::uint64_t value) : value_(value) {}

  // returns size in bytes
  std::uint8_t size() const noexcept {
    if (value_ < (1 << 6))
      return 1;
    else if (value_ < (1 << 14))
      return 2;
    else if (value_ < (1 << 30))
      return 4;
    else
      return 8;
  }

  // Arthematic operators
  ////////////////////////////////////////////////////////////////
  // Conversion operator to std::uint64_t
  operator std::uint64_t() const noexcept { return value_; }

  const std::uint64_t &get() const noexcept { return value_; }

  std::uint64_t &get() noexcept { return value_; }
};

} // namespace rvn::ds
