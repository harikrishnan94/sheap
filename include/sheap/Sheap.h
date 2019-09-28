#pragma once

#include "detail/utils.h"

#include <thread>

namespace sheap {
struct config {
  const int max_threads = -1;
  const std::size_t page_size = 64 * 1024;
  const std::size_t num_heaps =
      detail::next_pow_2(std::thread::hardware_concurrency() * 4);

  explicit config(int max_threads) : max_threads(max_threads) {}
  config(int max_threads, std::size_t page_size, std::size_t num_heaps)
      : max_threads(max_threads), page_size(page_size), num_heaps(num_heaps) {}
};

class Sheap {
public:
  explicit Sheap(void *mem, std::size_t size, const config &c);

  void *alloc(std::size_t size) noexcept;
  void *alloc(int tid, std::size_t size) noexcept;
  void free(void *ptr) noexcept;
  void collect_garbage() noexcept;
  void collect_all_garbage() noexcept;

  template <typename T, typename... Args> T *construct(Args... args) {
    static_assert(alignof(T) <= 16);
    if (auto mem = alloc(sizeof(T)))
      return new (mem) T{std::forward<Args>(args)...};
    else
      return nullptr;
  }
  template <typename T, typename... Args> T *construct(int tid, Args... args) {
    static_assert(alignof(T) <= 16);
    if (auto mem = alloc(tid, sizeof(T)))
      return new (mem) T{std::forward<Args>(args)...};
    else
      return nullptr;
  }

  template <typename T> void destruct(T *ptr) noexcept {
    static_assert(alignof(T) <= 16);
    ptr->~T();
    free(static_cast<void *>(ptr));
  }

private:
  struct impl;

  static impl *create(void *mem, std::size_t size, const config &c);

  impl *m_imp;
};

} // namespace sheap