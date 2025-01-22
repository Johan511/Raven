#pragma once

#include <cstdint>

namespace rvn::ds
{
/*
// integer which is variable length encoded
Length determination based on the top two bits of the first byte:
00 => 1 byte total (6 bits of payload)
01 => 2 bytes total (14 bits of payload)
10 => 4 bytes total (30 bits of payload)
11 => 8 bytes total (62 bits of payload)
*/
class quic_var_int
{
    std::uint64_t value_{};

public:
    quic_var_int(std::uint64_t value) : value_(value)
    {
    }

    // returns size in bytes
    std::uint8_t size() const
    {
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
    operator std::uint64_t() const
    {
        return value_;
    }

    std::uint64_t value() const
    {
        return value_;
    }

    // Arithmetic operators
    quic_var_int& operator+=(const quic_var_int& other)
    {
        value_ += other.value_;
        return *this;
    }
    quic_var_int& operator-=(const quic_var_int& other)
    {
        value_ -= other.value_;
        return *this;
    }
    quic_var_int& operator*=(const quic_var_int& other)
    {
        value_ *= other.value_;
        return *this;
    }
    quic_var_int& operator/=(const quic_var_int& other)
    {
        value_ /= other.value_;
        return *this;
    }
    quic_var_int& operator%=(const quic_var_int& other)
    {
        value_ %= other.value_;
        return *this;
    }

    quic_var_int operator+(const quic_var_int& other) const
    {
        return quic_var_int(value_ + other.value_);
    }
    quic_var_int operator-(const quic_var_int& other) const
    {
        return quic_var_int(value_ - other.value_);
    }
    quic_var_int operator*(const quic_var_int& other) const
    {
        return quic_var_int(value_ * other.value_);
    }
    quic_var_int operator/(const quic_var_int& other) const
    {
        return quic_var_int(value_ / other.value_);
    }
    quic_var_int operator%(const quic_var_int& other) const
    {
        return quic_var_int(value_ % other.value_);
    }

    // Comparison operators
    bool operator==(const quic_var_int& other) const
    {
        return value_ == other.value_;
    }
    bool operator!=(const quic_var_int& other) const
    {
        return value_ != other.value_;
    }
    bool operator<(const quic_var_int& other) const
    {
        return value_ < other.value_;
    }
    bool operator<=(const quic_var_int& other) const
    {
        return value_ <= other.value_;
    }
    bool operator>(const quic_var_int& other) const
    {
        return value_ > other.value_;
    }
    bool operator>=(const quic_var_int& other) const
    {
        return value_ >= other.value_;
    }

    // Increment and decrement operators
    quic_var_int& operator++()
    {
        ++value_;
        return *this;
    }
    quic_var_int operator++(int)
    {
        quic_var_int temp = *this;
        ++value_;
        return temp;
    }
    quic_var_int& operator--()
    {
        --value_;
        return *this;
    }
    quic_var_int operator--(int)
    {
        quic_var_int temp = *this;
        --value_;
        return temp;
    }
    ////////////////////////////////////////////////////////////////
};

} // namespace rvn::ds
