#pragma once

#include "Page.h"

namespace sheap::detail {
class ThreadCache {
public:
  ThreadCache() : m_active(Page::get_null_page()) {}

  template <bool IsAlignedAlloc, typename PageAlloc, typename PageFree>
  void *alloc(PageAlloc &&page_alloc, PageFree &&page_free) noexcept {
    if (auto mem = alloc_fast<IsAlignedAlloc>())
      return mem;

    if (auto mem = alloc_slow<IsAlignedAlloc>())
      return mem;

    return alloc_very_slow<IsAlignedAlloc>(page_alloc, page_free);
  }

private:
  template <bool IsAlignedAlloc> void *alloc_fast() {
    if (auto mem = m_active->alloc(); BOOST_LIKELY(mem != nullptr)) {
      if constexpr (IsAlignedAlloc) {
        m_active->set_has_aligned();
      }
      return mem;
    }

    return nullptr;
  }

  template <bool IsAlignedAlloc> void *alloc_slow() {
    if (BOOST_LIKELY(!m_active->is_null())) {
      m_used_pages.push_front(*m_active);
      m_active = Page::get_null_page();
    }

    if (BOOST_LIKELY(!m_rem_pages.empty())) {
      m_active = &m_rem_pages.front();
      m_rem_pages.pop_front();
    }

    return alloc_fast<IsAlignedAlloc>();
  }

  template <bool IsAlignedAlloc, typename PageAlloc, typename PageFree>
  void *alloc_very_slow(PageAlloc &&page_alloc, PageFree &&page_free) {
    BOOST_ASSERT(m_active->is_null());
    BOOST_ASSERT(m_rem_pages.empty());

    if (!m_used_pages.empty())
      page_free(m_used_pages);

    m_rem_pages = page_alloc();
    return alloc_slow<IsAlignedAlloc>();
  }

  Page *m_active = nullptr;
  FreePageList m_rem_pages = {};
  FreePageList m_used_pages = {};
};
} // namespace sheap::detail