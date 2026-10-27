// Unaligned little-endian scalar access for file formats, independent of host byte order.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace aztec {

template <class T> T read_le(const uint8_t* p) {
  static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
  static_assert(sizeof(T) <= 8);
  using Bits = std::conditional_t<sizeof(T) == 8, uint64_t,
               std::conditional_t<sizeof(T) == 4, uint32_t,
               std::conditional_t<sizeof(T) == 2, uint16_t, uint8_t>>>;
  Bits bits = 0;
  for (size_t i = 0; i < sizeof(T); ++i) bits |= Bits(p[i]) << (8 * i);
  if constexpr (std::is_floating_point_v<T>) {
    static_assert(std::numeric_limits<T>::is_iec559);
    T value;
    std::memcpy(&value, &bits, sizeof(T));
    return value;
  } else {
    return static_cast<T>(bits);
  }
}

template <class T> void write_le(uint8_t* p, T value) {
  static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
  static_assert(sizeof(T) <= 8);
  using Bits = std::conditional_t<sizeof(T) == 8, uint64_t,
               std::conditional_t<sizeof(T) == 4, uint32_t,
               std::conditional_t<sizeof(T) == 2, uint16_t, uint8_t>>>;
  Bits bits;
  if constexpr (std::is_floating_point_v<T>) {
    static_assert(std::numeric_limits<T>::is_iec559);
    std::memcpy(&bits, &value, sizeof(T));
  } else {
    bits = static_cast<Bits>(value);
  }
  for (size_t i = 0; i < sizeof(T); ++i) p[i] = uint8_t(bits >> (8 * i));
}

}  // namespace aztec
