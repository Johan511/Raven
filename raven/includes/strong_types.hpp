#pragma once
#include <cstdint>
namespace rvn
{

namespace detail
{
    template <typename ValueType, typename StrongTypeTag, template <typename, typename> typename... Traits>
    class StrongTypeImpl
    : public Traits<ValueType, StrongTypeImpl<ValueType, StrongTypeTag, Traits...>>...
    {
    public:
        ValueType value_;

        explicit StrongTypeImpl(ValueType value) : value_(value)
        {
        }
        explicit StrongTypeImpl() : value_()
        {
        }
        ValueType& get()
        {
            return value_;
        }
        const ValueType& get() const
        {
            return value_;
        }
    };

    template <typename ValueType, typename Derived> class UintCTRPTrait
    {
    private:
        ValueType& get() noexcept
        {
            return static_cast<Derived&>(*this).get();
        }

        const ValueType& get() const noexcept
        {
            return static_cast<const Derived&>(*this).get();
        }

    public:
        operator ValueType() const noexcept
        {
            return get();
        }

        ValueType hash() const noexcept
        {
            return get();
        }
        // Arthematic operators
        ////////////////////////////////////////////////////////////////
        Derived& operator+=(const Derived& other) noexcept
        {
            get() += other.get();
            return *this;
        }
        Derived& operator-=(const Derived& other) noexcept
        {
            get() -= other.get();
            return *this;
        }
        Derived& operator*=(const Derived& other) noexcept
        {
            get() *= other.get();
            return *this;
        }
        Derived& operator/=(const Derived& other) noexcept
        {
            get() /= other.get();
            return *this;
        }
        Derived& operator%=(const Derived& other) noexcept
        {
            get() %= other.get();
            return *this;
        }

        Derived operator+(const Derived& other) const noexcept
        {
            return Derived(get() + other.get());
        }
        Derived operator-(const Derived& other) const noexcept
        {
            return Derived(get() - other.get());
        }
        Derived operator*(const Derived& other) const noexcept
        {
            return Derived(get() * other.get());
        }
        Derived operator/(const Derived& other) const noexcept
        {
            return Derived(get() / other.get());
        }
        Derived operator%(const Derived& other) const noexcept
        {
            return Derived(get() % other.get());
        }

        // Comparison operators
        bool operator==(const Derived& other) const noexcept
        {
            return get() == other.get();
        }
        bool operator!=(const Derived& other) const noexcept
        {
            return get() != other.get();
        }
        bool operator<(const Derived& other) const noexcept
        {
            return get() < other.get();
        }
        bool operator<=(const Derived& other) const noexcept
        {
            return get() <= other.get();
        }
        bool operator>(const Derived& other) const noexcept
        {
            return get() > other.get();
        }
        bool operator>=(const Derived& other) const noexcept
        {
            return get() >= other.get();
        }

        // Increment and decrement operators
        Derived& operator++() noexcept
        {
            ++get();
            return *this;
        }
        Derived operator++(int) noexcept
        {
            Derived temp = *this;
            ++get();
            return temp;
        }
        Derived& operator--() noexcept
        {
            --get();
            return *this;
        }
        Derived operator--(int) noexcept
        {
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

using ObjectId = detail::StrongTypeImpl<std::uint64_t, ObjectIdTag, detail::UintCTRPTrait>;
using GroupId = detail::StrongTypeImpl<std::uint64_t, GroupIdTag, detail::UintCTRPTrait>;
using SubGroupId = detail::StrongTypeImpl<std::uint64_t, SubGroupIdTag, detail::UintCTRPTrait>;
using TrackAlias = detail::StrongTypeImpl<std::uint64_t, TrackAliasTag, detail::UintCTRPTrait>;
// clang-format on

}; // namespace rvn
