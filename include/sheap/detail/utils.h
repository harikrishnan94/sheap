#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace sheap::detail {
static constexpr int log2(std::size_t n) {
  int lg2 = 0;
  while (n >>= 1) {
    ++lg2;
  }
  return lg2;
}

static constexpr bool is_pow2(std::size_t n) { return !(n & (n - 1)); }
static constexpr std::size_t pow2(int n) { return UINT64_C(1) << n; }

static constexpr int next_pow_2(std::size_t n) {
  if (n <= 1) {
    return 1;
  }

  if (is_pow2(n)) {
    return n;
  }

  int log_2 = log2(n);

  return pow2(log_2 + 1);
}

template <
    typename T, typename... Args,
    typename = typename std::enable_if_t<std::is_constructible_v<T, Args...>>>
T *construct(T *ptr, Args... args) noexcept {
  return new (reinterpret_cast<char *>(ptr)) T{std::forward<Args>(args)...};
}

} // namespace sheap::detail