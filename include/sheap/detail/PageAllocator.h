#pragma once

#include "Page.h"
#include "SpinLock.h"

namespace sheap::detail {
class PageAllocator {
public:
  PageAllocator(Page *pagearr, std::size_t num_pages) noexcept
      : m_pagearr(pagearr), m_num_pages(num_pages) {
    BOOST_ASSERT(pagearr != nullptr);
    BOOST_ASSERT(num_pages != 0);
  }

  Page *alloc() noexcept {
    std::lock_guard lock{m_mtx};

    if (!m_freelist.empty()) {
      auto page = &m_freelist.front();
      m_freelist.pop_front();
      return page;
    }

    if (m_next_page != m_num_pages)
      return m_pagearr + m_next_page++;

    return nullptr;
  }

  void free(Page *page) noexcept {
    std::lock_guard lock{m_mtx};
    m_freelist.push_front(*page);
  }
  void free(FreePageList &fl) noexcept {
    if (fl.empty())
      return;

    std::lock_guard lock{m_mtx};
    m_freelist.incorporate_after(
        m_freelist.before_begin(), fl.front().free_list_hook::this_ptr(),
        fl.back().free_list_hook::this_ptr(), fl.size());
  }

private:
  Page *const m_pagearr;
  FreePageList m_freelist = {};
  const std::size_t m_num_pages;
  std::size_t m_next_page = 0;
  SpinLock m_mtx = {};
};
} // namespace sheap::detail