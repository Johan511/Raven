#include <cstdint>
#include <iostream>
#include <limits>
#include <serialization/serialization.hpp>

using namespace rvn::serialization;

template <typename IntegralType> void byte_sized_tests()
{
    std::cout << "Testing " << sizeof(IntegralType)
              << " byte values from:" << std::numeric_limits<IntegralType>::min()
              << " to " << std::numeric_limits<IntegralType>::max() << std::endl;
    IntegralType value = std::numeric_limits<IntegralType>::min();
    rvn::ds::chunk c(sizeof(IntegralType));
    do
    {

        detail::serialize_trivial<IntegralType>(c, value);

        IntegralType deserialized;
        detail::deserialize_trivial<IntegralType>(deserialized, c);

        assert(value == deserialized);
        c.clear();
    } while (value++ != std::numeric_limits<IntegralType>::max());
}

void bit_64_test_impl(std::uint64_t value)
{
    std::cout << "Testing " << sizeof(value) << " byte value: " << value << std::endl;
    rvn::ds::chunk c(sizeof(std::uint64_t));
    detail::serialize_trivial<std::uint64_t>(c, value);

    std::uint64_t deserialized;
    detail::deserialize_trivial<std::uint64_t>(deserialized, c);

    assert(value == deserialized);
}

void bit_64_test()
{
    bit_64_test_impl(0);
    bit_64_test_impl(~0);

    bit_64_test_impl(1);
    bit_64_test_impl(~1);

    bit_64_test_impl(2);
    bit_64_test_impl(~2);

    bit_64_test_impl(0x1234567890abcdef);
    bit_64_test_impl(~0x1234567890abcdef);

    bit_64_test_impl(0x0a0a0a0a0a0a0a0a);
    bit_64_test_impl(~0x0a0a0a0a0a0a0a0a);

    bit_64_test_impl(0x0000000a0a0a0a0a);
    bit_64_test_impl(~0x0000000a0a0a0a0a);
}

void byte_sized_tests()
{
    byte_sized_tests<std::uint8_t>();
    byte_sized_tests<std::uint16_t>();
    bit_64_test();
}


int main()
{
    byte_sized_tests();
    return 0;
}
