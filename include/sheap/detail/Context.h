#pragma once

#include "Page.h"
#include "utils.h"

#include <atomic>
#include <cstddef>

namespace sheap::detail {
class object {
public:
  static inline object *from(void *p) { return static_cast<object *>(p); }

  inline void poison(std::size_t size) {
    asan_poison_memory_region(this, size);
  }

  inline void unpoison() { asan_unpoison_memory_region(this, sizeof(*this)); }
  inline void poison() { asan_poison_memory_region(this, sizeof(*this)); }

  inline void set_next(object *n) {
    unpoison();
    next.store(n, std::memory_order_relaxed);
    poison();
  }
  inline object *get_next() {
    unpoison();
    auto n = next.load(std::memory_order_relaxed);
    poison();

    return n;
  }

private:
  std::atomic<object *> next;
};

class Context {
public:
  Context(Page *pages, std::size_t num_pages, std::size_t page_size, void *base)
      : m_sizeclasses(create_sizeclasses(page_size)), m_pages(pages),
        m_page_size(page_size), m_log_page_size(log2(page_size)), m_base(base)
#ifndef BOOST_ASSERT_IS_VOID
        ,
        m_num_pages(num_pages)
#endif
  {
    asan_poison_memory_region(pages, sizeof(Page) * num_pages);
    asan_poison_memory_region(base, page_size * num_pages);
  }

  void *get_page_ptr(const Page *page) const noexcept {
    auto pageno = get_pageno(page);

    BOOST_ASSERT(pageno < m_num_pages);
    return static_cast<void *>(static_cast<std::byte *>(m_base) +
                               pageno * m_page_size);
  }

  template <typename Ptr> Page *get_page(Ptr obj) const noexcept {
    auto pageno = get_pageno(obj);

    BOOST_ASSERT(pageno < m_num_pages);
    return m_pages + pageno;
  }

  [[nodiscard]] const SizeClass &get_size_class(int binid) const noexcept {
    return m_sizeclasses[binid];
  }

private:
  using SizeClassArray = std::array<SizeClass, NUM_BINS>;

  SizeClassArray create_sizeclasses(std::size_t page_size) {
    SizeClassArray sizeclasses;

    for (int i = 0; i < static_cast<int>(NUM_BINS); i++) {
      sizeclasses[i] = {i, Bins[i], page_size};
    }

    return sizeclasses;
  }
  inline std::size_t get_pageno(const Page *page) const {
    return page - m_pages;
  }

  template <typename Ptr> inline std::size_t get_pageno(Ptr obj) const {
    auto page = reinterpret_cast<std::uintptr_t>(obj) & ~(m_page_size - 1);
    auto page_base = reinterpret_cast<std::uintptr_t>(m_base);
    return (page - page_base) >> m_log_page_size;
  }

  const SizeClassArray m_sizeclasses;
  Page *const m_pages;
  const std::size_t m_page_size;
  const int m_log_page_size;
  void *m_base;
#ifndef BOOST_ASSERT_IS_VOID
  const std::size_t m_num_pages;
#endif
};
} // namespace sheap::detail