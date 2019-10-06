#pragma once

#include "SizeClass.h"
#include "utils.h"

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/pool/simple_segregated_storage.hpp>

namespace sheap::detail {
using normal_link = boost::intrusive::link_mode<boost::intrusive::normal_link>;
using auto_unlink = boost::intrusive::link_mode<boost::intrusive::auto_unlink>;
using page_list_hook = boost::intrusive::list_base_hook<auto_unlink>;
using free_list_hook = boost::intrusive::slist_base_hook<normal_link>;

class Heap;
class Page : public page_list_hook, public free_list_hook {
public:
  Page(const Page &) = delete;
  Page(Page &&) = delete;

  void init(const SizeClass &szc, void *page_base, Heap *heap) noexcept {
    asan_unpoison_memory_region(this, sizeof(*this));
    new (this) Page{szc, page_base, heap};
  }

  static inline Page *get_null_page() noexcept {
    static Page page;
    return &page;
  }

  [[nodiscard]] bool is_null() const noexcept {
    return this == get_null_page();
  }

  void *alloc() noexcept {
    BOOST_ASSERT(m_szc != nullptr ? m_num_free <= m_szc->num_objs : true);

    if (BOOST_UNLIKELY(m_freelist.empty()))
      return nullptr;

    BOOST_ASSERT(this != Page::get_null_page());
    BOOST_ASSERT(m_num_free > 0);
    m_num_free--;
    return alloc(m_freelist);
  }
  void free(void *obj) noexcept {
    BOOST_ASSERT(m_num_free != m_szc->num_objs);
    m_freelist.free(obj);
    m_num_free++;
  }

  [[nodiscard]] bool is_empty() const noexcept {
    return m_num_free == m_szc->num_objs;
  }
  [[nodiscard]] bool is_full() const noexcept { return m_num_free == 0; }
  [[nodiscard]] std::size_t num_free() const noexcept { return m_num_free; }
  [[nodiscard]] Heap *get_heap() const noexcept { return m_heap; }
  [[nodiscard]] const SizeClass &get_size_class() const noexcept {
    return *m_szc;
  }
  [[nodiscard]] bool is_in_heap() const noexcept { return m_is_in_heap; }
  void move_into_heap() noexcept { m_is_in_heap = true; }
  void move_outof_heap() noexcept { m_is_in_heap = false; }

private:
  Page() = default;
  Page(const SizeClass &szc, void *page_base, Heap *heap) noexcept
      : m_szc(&szc), m_num_free(szc.num_objs), m_heap(heap) {
    BOOST_ASSERT(m_freelist.empty());
    asan_unpoison_memory_region(page_base, szc.page_size);
    m_freelist.add_block(page_base, szc.page_size, szc.bin.size);
    asan_poison_memory_region(page_base, szc.page_size);
  }

  static inline void *alloc(boost::simple_segregated_storage<int> &freelist) {
    // Moasam...
    void *first = *reinterpret_cast<void **>(&freelist);
    asan_unpoison_memory_region(first, sizeof(void *));
    auto ret = freelist.malloc();
    asan_poison_memory_region(first, sizeof(void *));
    return ret;
  }

  boost::simple_segregated_storage<int> m_freelist = {};
  const SizeClass *m_szc = nullptr;
  std::size_t m_num_free = 0;
  Heap *const m_heap = nullptr;
  bool m_is_in_heap = false;
};

using FreePageList =
    boost::intrusive::slist<Page, boost::intrusive::linear<false>,
                            boost::intrusive::cache_last<true>>;
using PageList =
    boost::intrusive::list<Page, boost::intrusive::linear<false>,
                           boost::intrusive::constant_time_size<false>>;

} // namespace sheap::detail