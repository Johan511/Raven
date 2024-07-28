#pragma once

#include <msquic.h>

#include <iostream>
#include <ostream>
#include <utility>

namespace rvn::utils {
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(
    E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(
        e);
}

template <typename T>
static void print(std::ostream& os, T value) {
    os << value << std::endl;
}

template <typename T, typename... Args>
static void print(std::ostream& os, T value, Args... args) {
    os << value << " ";
    print(os, args...);
}

template <typename... Args>
static void ASSERT_LOG_THROW(bool assertion, Args... args) {
    if (!assertion) {
        print(std::cerr, args...);
        throw std::runtime_error("Assertion failed");
    }
}

}  // namespace rvn::utils