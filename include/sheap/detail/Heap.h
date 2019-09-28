#pragma once

#include "Context.h"
#include "Page.h"
#include "PageAllocator.h"
#include "SpinLock.h"

#include <array>
#include <atomic>
#include <utility>

namespace sheap::detail {
constexpr auto MIN_FREE_OBJS = 50;

class UsedPageStorage {
public:
  std::pair<FreePageList, FreePageList> alloc(Context &cxt) noexcept {
    auto purgable_pages = get_purgable_pages(cxt);
    std::lock_guard lock{m_mtx};
    return {get_partial_pages(), std::move(purgable_pages)};
  }

  void push_full_pages(FreePageList &pages) noexcept {
    std::lock_guard lock{m_mtx};
    for (auto it = pages.begin(), end = pages.end(); it != end;) {
      auto &page = *it;
      it = pages.erase(it);
      PageList::node_algorithms::init(&page);
      m_full_pages.push_back(page);
      page.move_into_heap();
    }
  }

  void deferred_free(object *obj) noexcept {
    while (true) {
      auto old = m_deferred_free.load();
      obj->next.store(old, std::memory_order_relaxed);

      if (m_deferred_free.compare_exchange_strong(old, obj))
        return;
    }
  }

  FreePageList get_purgable_pages(Context &cxt) {
    std::lock_guard lock{m_mtx};
    auto *deferred = get_deferred();
    auto [purgable_pages, deferred_again] = apply_deferred_free(deferred, cxt);

    if (deferred_again) {
      while (true) {
        auto old = m_deferred_free.load(std::memory_order_acquire);
        deferred_again->next.store(old, std::memory_order_release);

        if (m_deferred_free.compare_exchange_strong(deferred, deferred_again))
          break;
      }
    }

    return std::move(purgable_pages);
  }

private:
  FreePageList get_partial_pages() {
    std::size_t num_objs = 0;
    FreePageList pages;

    while (!m_partial_pages.empty() && num_objs < MIN_FREE_OBJS) {
      auto &page = m_partial_pages.front();
      BOOST_ASSERT(!page.is_empty() && !page.is_full());
      BOOST_ASSERT(page.is_in_heap());

      page.move_outof_tcache();
      m_partial_pages.pop_front();
      pages.push_front(page);
      num_objs += page.num_free();
    }

    return pages;
  }

  object *get_deferred() {
    while (true) {
      auto *freed = m_deferred_free.load(std::memory_order_acquire);

      if (freed == nullptr)
        return freed;

      if (m_deferred_free.compare_exchange_strong(freed, nullptr))
        return freed;
    }
  }

  std::pair<FreePageList, object *> apply_deferred_free(object *deferred,
                                                        Context &cxt) {
    FreePageList purgable_pages;
    object *deferred_again = nullptr;

    for (auto obj = deferred; obj;) {
      auto next = obj->next.load(std::memory_order_relaxed);
      auto page = cxt.get_page(obj);

      if (free(obj, cxt)) {
        if (page->is_empty()) {
          BOOST_ASSERT(page->page_list_hook::is_linked());
          page->page_list_hook::unlink();
          page->move_outof_tcache();
          purgable_pages.push_back(*page);
        }
      } else {
        obj->next.store(deferred_again, std::memory_order_relaxed);
        deferred_again = obj;
      }

      obj = next;
    }

    std::atomic_thread_fence(std::memory_order_release);

    return {std::move(purgable_pages), deferred_again};
  }

  bool free(object *obj, Context &cxt) {
    auto page = cxt.get_page(obj);

    if (!page->is_in_heap())
      return false;

    auto was_full = page->is_full();

    page->free(obj);
    if (was_full && !page->is_empty()) {
      BOOST_ASSERT(page->page_list_hook::is_linked());
      page->page_list_hook::unlink();
      m_partial_pages.push_back(*page);
    }

    return true;
  }

  PageList m_full_pages = {};
  PageList m_partial_pages = {};
  std::atomic<object *> m_deferred_free = {};
  SpinLock m_mtx = {};
};

class Heap {
public:
  Heap(Context &cxt, PageAllocator &page_alloc)
      : m_cxt(cxt), m_page_alloc(page_alloc) {}

  FreePageList alloc_pages(int bin_id) noexcept {
    if (auto pages = alloc_partial_pages(bin_id); BOOST_LIKELY(!pages.empty()))
      return pages;

    if (auto pages = alloc_from_cache(bin_id); BOOST_LIKELY(!pages.empty()))
      return pages;

    return alloc_fresh_pages(bin_id);
  }

  void push_full_pages(int bin_id, FreePageList &pages) noexcept {
    m_ups[bin_id].push_full_pages(pages);
  }

  void deferred_free(int bin_id, void *obj) noexcept {
    m_ups[bin_id].deferred_free(static_cast<object *>(obj));
  }

  void collect_garbage(bool flush_cache) noexcept {
    for (auto &ps : m_ups) {
      auto pages = ps.get_purgable_pages(m_cxt);
      purge_pages(pages);
    }

    if (flush_cache)
      clear_cache();
  }

private:
  FreePageList alloc_partial_pages(int bin_id) {
    auto [pages, purgable_pages] = m_ups[bin_id].alloc(m_cxt);

    purge_pages(purgable_pages);
    return std::move(pages);
  }

  FreePageList alloc_from_cache(int bin_id) {
    std::lock_guard lock{m_cache_mtx};
    FreePageList pages;
    int num_objs = 0;

    while (!m_free_page_cache.empty() && num_objs < MIN_FREE_OBJS) {
      auto &page = m_free_page_cache.front();
      BOOST_ASSERT(page.is_empty());
      BOOST_ASSERT(!page.is_in_heap());

      m_free_page_cache.pop_front();
      page.init(m_cxt.get_size_class(bin_id), m_cxt.get_page_ptr(&page), this);
      pages.push_front(page);
      num_objs += page.num_free();
    }

    return pages;
  }

  FreePageList alloc_fresh_pages(int bin_id) {
    FreePageList pages;
    int num_objs = 0;

    while (num_objs < MIN_FREE_OBJS) {
      auto page = m_page_alloc.alloc();

      if (page == nullptr)
        break;

      page->init(m_cxt.get_size_class(bin_id), m_cxt.get_page_ptr(page), this);
      pages.push_front(*page);
      num_objs += page->num_free();
    }

    return pages;
  }

  void purge_pages(FreePageList &pages) {
    if (pages.empty())
      return;

    std::lock_guard lock{m_cache_mtx};
    for (auto size = m_free_page_cache.size();
         !pages.empty() && size < NUM_CACHED_PAGES; size++) {
      auto &page = pages.front();
      BOOST_ASSERT(page.is_empty());
      BOOST_ASSERT(!page.is_in_heap());
      pages.pop_front();
      m_free_page_cache.push_front(page);
    }
    m_page_alloc.free(pages);
  }

  void clear_cache() {
    FreePageList pages;
    std::lock_guard lock{m_cache_mtx};

    while (!m_free_page_cache.empty()) {
      auto &page = m_free_page_cache.front();
      BOOST_ASSERT(page.is_empty());
      BOOST_ASSERT(!page.is_in_heap());
      m_free_page_cache.pop_front();
      pages.push_front(page);
    }

    m_page_alloc.free(pages);
  }

  Context &m_cxt;
  std::array<UsedPageStorage, NUM_BINS> m_ups;
  PageAllocator &m_page_alloc;

  FreePageList m_free_page_cache = {};
  SpinLock m_cache_mtx = {};

  static constexpr int NUM_CACHED_PAGES = 100;
};
} // namespace sheap::detail