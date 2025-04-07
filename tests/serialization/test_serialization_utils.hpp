#pragma once

#include <cstdint>
#include <string_view>
#include <utilities.hpp>
#include <vector>

/*
    We represent serialized data in bits (rather than hex) as it is easier to
   see the structure of the data. We want to convert the string of 0s and 1s to
   a vector of bytes (bit representation). Unline the hex representation
   ("\x0a\x0b"), we can not simply have a binary string Hence we use this
   helpter function

    The function ignores all charecters apart from the the charecters '0' and
   '1'
*/
static inline std::vector<uint8_t> binary_string_to_vector(std::string_view str)
{
    std::vector<uint8_t> vec;
    vec.reserve((str.size() + 8) / 8);

    std::uint8_t currByte = 0;
    std::uint8_t bitCount = 0;
    for (char c : str)
    {
        if (c == '0')
        {
            currByte = currByte << 1;
            bitCount++;
        }
        else if (c == '1')
        {
            currByte = (currByte << 1) | 1;
            bitCount++;
        }

        if (bitCount == 8)
        {
            vec.push_back(currByte);
            currByte = 0;
            bitCount = 0;
        }
    }

    rvn::utils::ASSERT_LOG_THROW(bitCount == 0, "Invalid binary string, not multiple of 8\n",
                                 "Trailing bit count: ", bitCount, "\n",
                                 "Binary String: \n", str);
    return vec;
}
