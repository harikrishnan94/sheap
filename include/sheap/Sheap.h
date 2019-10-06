#pragma once

#include "detail/utils.h"
#include "sheap/detail/SizeClass.h"

#include <boost/align/align_up.hpp>
#include <thread>
#include <utility>

namespace sheap {
struct config {
  const int max_threads = -1;
  const std::size_t page_size = 64 * 1024;
  const std::size_t num_heaps =
      detail::next_pow_2(std::thread::hardware_concurrency() * 4);

  explicit config(int max_threads) : max_threads(max_threads) {}
  constexpr config(int max_threads, std::size_t page_size,
                   std::size_t num_heaps)
      : max_threads(max_threads), page_size(page_size), num_heaps(num_heaps) {}
};

template <bool Value> struct flush_cache {
  static constexpr auto value = Value ? 0x1 : 0;
};

class Sheap {
public:
  explicit Sheap(void *mem, std::size_t size, const config &c);
  Sheap(Sheap &&o) : m_imp(std::exchange(o.m_imp, nullptr)) {}

  Sheap(const Sheap &) = delete;

  void *alloc(int tid, std::size_t size) noexcept;
  void *aligned_alloc(int tid, std::size_t size, std::size_t align) noexcept;
  void free(void *ptr) noexcept;

  template <typename FlushCache = flush_cache<false>>
  void collect_garbage(int tid = -1) noexcept {
    collect_garbage(tid, FlushCache::value);
  }
  void collect_garbage_full() noexcept { collect_garbage(-1, true); }

  template <typename T, typename... Args> T *construct(int tid, Args... args) {
    static_assert(sizeof(T) <= max_alloc_size(),
                  "cannot allocate more than max_alloc_size()");

    constexpr auto binid = detail::BinMap[sizeof(T)];
    if constexpr (detail::Bins[binid].alignment == alignof(T)) {
      if (auto mem = alloc(tid, sizeof(T)))
        return new (mem) T{std::forward<Args>(args)...};
    } else {
      static_assert(sizeof(T) + alignof(T) <= max_alloc_size(),
                    "cannot allocate more than max_alloc_size()");

      if (auto mem = aligned_alloc(tid, sizeof(T), alignof(T)))
        return new (mem) T{std::forward<Args>(args)...};
    }

    return nullptr;
  }

  template <typename T> void destruct(T *ptr) noexcept {
    static_assert(alignof(T) <= max_alloc_size());
    ptr->~T();
    free(static_cast<void *>(ptr));
  }

  static constexpr std::size_t max_alloc_size() { return detail::MaxAllocSize; }

private:
  struct impl;

  static impl *create(void *mem, std::size_t size, const config &c);
  void collect_garbage(int tid, bool flush_cache) noexcept;

  impl *m_imp;
};

} // namespace sheap