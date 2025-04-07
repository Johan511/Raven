#pragma once
#include <cstdint>
namespace rvn {

namespace detail {
template <typename ValueType, typename StrongTypeTag,
          template <typename, typename> typename... Traits>
class StrongTypeImpl
    : public Traits<ValueType,
                    StrongTypeImpl<ValueType, StrongTypeTag, Traits...>>... {
public:
  ValueType value_;

  constexpr explicit StrongTypeImpl(ValueType value) : value_(value) {}
  constexpr explicit StrongTypeImpl() : value_() {}
  constexpr ValueType &get() { return value_; }
  constexpr const ValueType &get() const { return value_; }
};

template <typename ValueType, typename Derived> class UintCTRPTrait {
private:
  constexpr ValueType &get() noexcept {
    return static_cast<Derived &>(*this).get();
  }

  constexpr const ValueType &get() const noexcept {
    return static_cast<const Derived &>(*this).get();
  }

public:
  constexpr operator ValueType() const noexcept { return get(); }

  constexpr ValueType hash() const noexcept { return get(); }
  // Arthematic operators
  ////////////////////////////////////////////////////////////////
  constexpr Derived &operator+=(const Derived &other) noexcept {
    get() += other.get();
    return *this;
  }
  constexpr Derived &operator-=(const Derived &other) noexcept {
    get() -= other.get();
    return *this;
  }
  constexpr Derived &operator*=(const Derived &other) noexcept {
    get() *= other.get();
    return *this;
  }
  constexpr Derived &operator/=(const Derived &other) noexcept {
    get() /= other.get();
    return *this;
  }
  constexpr Derived &operator%=(const Derived &other) noexcept {
    get() %= other.get();
    return *this;
  }

  constexpr Derived operator+(const Derived &other) const noexcept {
    return Derived(get() + other.get());
  }
  constexpr Derived operator-(const Derived &other) const noexcept {
    return Derived(get() - other.get());
  }
  constexpr Derived operator*(const Derived &other) const noexcept {
    return Derived(get() * other.get());
  }
  constexpr Derived operator/(const Derived &other) const noexcept {
    return Derived(get() / other.get());
  }
  constexpr Derived operator%(const Derived &other) const noexcept {
    return Derived(get() % other.get());
  }

  // Comparison operators
  constexpr bool operator==(const Derived &other) const noexcept {
    return get() == other.get();
  }
  constexpr bool operator!=(const Derived &other) const noexcept {
    return get() != other.get();
  }
  constexpr bool operator<(const Derived &other) const noexcept {
    return get() < other.get();
  }
  constexpr bool operator<=(const Derived &other) const noexcept {
    return get() <= other.get();
  }
  constexpr bool operator>(const Derived &other) const noexcept {
    return get() > other.get();
  }
  constexpr bool operator>=(const Derived &other) const noexcept {
    return get() >= other.get();
  }

  // Increment and decrement operators
  constexpr Derived &operator++() noexcept {
    ++get();
    return *this;
  }
  constexpr Derived operator++(int) noexcept {
    Derived temp = *this;
    ++get();
    return temp;
  }
  constexpr Derived &operator--() noexcept {
    --get();
    return *this;
  }
  constexpr Derived operator--(int) noexcept {
    Derived temp = *this;
    --get();
    return temp;
  }
  ////////////////////////////////////////////////////////////////
};
} // namespace detail

// clang-format off
struct ObjectIdTag{};
struct GroupIdTag{};
struct SubGroupIdTag{};
struct TrackAliasTag{};
struct SubscriberPriorityTag{};
struct PublisherPriorityTag{};

using ObjectId = detail::StrongTypeImpl<std::uint64_t, ObjectIdTag, detail::UintCTRPTrait>;
using GroupId = detail::StrongTypeImpl<std::uint64_t, GroupIdTag, detail::UintCTRPTrait>;
using SubGroupId = detail::StrongTypeImpl<std::uint64_t, SubGroupIdTag, detail::UintCTRPTrait>;
using TrackAlias = detail::StrongTypeImpl<std::uint64_t, TrackAliasTag, detail::UintCTRPTrait>;

// MOQT priority values are 8 bit integers where as MsQuic supports 16 bit integers, be very careful when converting MsQuic priority to MOQT priority
// Low priority value means more important according to MOQT and MsQuic (ig so, https://github.com/microsoft/msquic/issues/4826)
using SubscriberPriority = detail::StrongTypeImpl<std::uint8_t, SubscriberPriorityTag, detail::UintCTRPTrait>;
using PublisherPriority = detail::StrongTypeImpl<std::uint8_t, PublisherPriorityTag, detail::UintCTRPTrait>;
// clang-format on

}; // namespace rvn
