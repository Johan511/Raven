#include "serialization/quic_var.hpp"
#include "utilities.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <serialization/deserialization_impl.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

template <typename IntegralType>
void byte_sized_tests(std::size_t from, std::size_t to) // [from, to)
{
    std::cout << "Testing " << sizeof(IntegralType)
              << " byte values from:" << from << " to " << to << std::endl;
    IntegralType value = std::numeric_limits<IntegralType>::min();
    rvn::ds::chunk c(sizeof(IntegralType));
    do
    {
        try
        {
            ds::quic_var_int i = value;
            detail::serialize<ds::quic_var_int>(c, i);

            ds::quic_var_int deserialized(0);
            detail::deserialize<ds::quic_var_int>(deserialized, c);

            utils::ASSERT_LOG_THROW(value == deserialized,
                                    "Failed to deserialize value: ", value,
                                    "Deserialized to ", deserialized);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to serialize/deserialize value: " << value << std::endl;
            std::cerr << e.what() << std::endl;
        }
        c.clear();
    } while (++value != to);
}

void bit_64_test_impl(std::uint64_t value)
{
    std::cout << "Testing " << sizeof(value) << " byte value: " << value << std::endl;
    rvn::ds::chunk c(sizeof(value));

    try
    {
        ds::quic_var_int i = value;
        detail::serialize<ds::quic_var_int>(c, i);

        ds::quic_var_int deserialized(0);
        detail::deserialize<ds::quic_var_int>(deserialized, c);

        utils::ASSERT_LOG_THROW(value == deserialized, "Failed to deserialize value: ", value,
                                "Deserialized to ", deserialized);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to serialize/deserialize value: " << value << std::endl;
        std::cerr << e.what() << std::endl;
    }
    c.clear();
}

void bit_64_test()
{
    bit_64_test_impl(0);

    bit_64_test_impl(1);

    bit_64_test_impl(2);

    bit_64_test_impl(0x0234567890abcdef);

    bit_64_test_impl(0x0a0a0a0a0a0a0a0a);

    bit_64_test_impl(0x0000000a0a0a0a0a);
}

void byte_sized_tests()
{
    byte_sized_tests<std::uint8_t>(0, 1 << 6);
    byte_sized_tests<std::uint16_t>(0, 1 << 14);
    byte_sized_tests<std::uint32_t>(0, 1 << 30);
    bit_64_test();
}


int main()
{
    byte_sized_tests();
    return 0;
}
