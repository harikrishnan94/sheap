#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <sanitizer/asan_interface.h>
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

template <typename Ptr> std::uintptr_t to_int(Ptr p) noexcept {
  return reinterpret_cast<std::size_t>(p);
}

template <typename Ptr = void *> Ptr to_ptr(std::uintptr_t p) noexcept {
  return reinterpret_cast<Ptr>(p);
}

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#define ASAN_ENABLED
#endif

inline void asan_poison_memory_region(void const volatile *addr, size_t size) {
#ifdef ASAN_ENABLED
  if (addr)
    ASAN_POISON_MEMORY_REGION(addr, size);
#endif
}

inline void asan_unpoison_memory_region(void const volatile *addr,
                                        size_t size) {
  ASAN_UNPOISON_MEMORY_REGION(addr, size);
}

} // namespace sheap::detail