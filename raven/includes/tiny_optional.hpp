#pragma once

#include <memory>

namespace rvn {

template <typename T, T FalseValue,
          auto OptionalFalsePred =
              [](const T *value) { return *value != FalseValue; }>
class TinyOptional {
  alignas(alignof(T)) char storage_[sizeof(T)];

public:
  TinyOptional() { new (storage_) T(FalseValue); }

  TinyOptional(const T &value) { new (storage_) T(value); }

  bool has_value() const {
    return OptionalFalsePred()(reinterpret_cast<const T *>(storage_));
  }

  T &value() { return *reinterpret_cast<T *>(storage_); }
  const T &value() const { return *reinterpret_cast<const T *>(storage_); }

  T &operator*() { return value(); }
  const T &operator*() const { return value(); }
  T *operator->() { return std::addressof(value()); }
  const T *operator->() const { return std::addressof(value()); }
  operator bool() const { return has_value(); }
};

} // namespace rvn
