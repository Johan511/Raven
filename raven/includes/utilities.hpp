#pragma once

#include <msquic.h>

#include <utility>

namespace rvn::utils {
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(
    E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(
        e);
}

}  // namespace rvn::utilities