#include <serialization/serialization.hpp>

// TODO: probably still UB
template <typename T> std::string represent_bytes(const T& t)
{
    static_assert(std::is_integral<T>::value, "T must be an integral type");
    union
    {
        T value;
        unsigned char bytes[sizeof(T)];
    } u;
    u.value = t;
    std::string result = "L ";
    for (std::size_t i = 0; i < sizeof(T); ++i)
        result += std::to_string(u.bytes[i]) + " ";
    result += "H\n"; // High address byte
    return result;
}


int main()
{
}
