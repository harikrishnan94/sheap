#include "sheap/Sheap.h"
#include "sheap/detail/Heap.h"
#include "sheap/detail/ThreadCache.h"

#include <cstdio>
#include <memory>
#include <thread>
#include <utility>

namespace sheap {
using namespace detail;
struct Sheap::impl {
  impl(Context &cxt, Heap *heaps, int num_heaps, ThreadCache **tcache,
       int max_threads)
      : m_cxt(cxt), m_heaps(heaps), m_num_heaps(num_heaps), m_tcache(tcache),
        m_max_threads(max_threads) {}
  impl(const impl &) = delete;
  impl(impl &&) = delete;

  const Context &m_cxt;
  Heap *const m_heaps;
  const int m_num_heaps;
  ThreadCache *const *const m_tcache;
  const int m_max_threads;
};

template <typename T>
static T *align(std::size_t count, void *&mem, std::size_t &space) {
  if (std::align(alignof(T), sizeof(T) * count, mem, space)) {
    auto res = static_cast<T *>(mem);
    mem = static_cast<void *>((static_cast<char *>(mem) + sizeof(T) * count));
    space -= sizeof(T) * count;
    return res;
  }

  throw std::bad_alloc{};
}

static inline ThreadCache **alloc_tcache(void *&mem, std::size_t &size,
                                         int max_threads) {
  auto tcache = align<ThreadCache *>(max_threads, mem, size);

  for (int i = 0; i < max_threads; i++) {
    tcache[i] = align<ThreadCache>(NUM_BINS, mem, size);

    for (int j = 0; j < NUM_BINS; j++)
      detail::construct(&tcache[i][j]);
  }

  return tcache;
}

Sheap::Sheap(void *mem, std::size_t size, const config &c)
    : m_imp(create(mem, size, c)) {}

Sheap::impl *Sheap::create(void *mem, std::size_t size, const config &c) {
  BOOST_ASSERT(c.max_threads > 0);

  auto num_heaps = detail::next_pow_2(c.num_heaps);
  auto max_threads = detail::next_pow_2(c.max_threads);

  auto imp = align<impl>(1, mem, size);
  auto cxt = align<Context>(1, mem, size);
  auto page_alloc = align<PageAllocator>(1, mem, size);
  auto heaps = align<Heap>(num_heaps, mem, size);
  auto tcache = alloc_tcache(mem, size, max_threads);
  auto num_pages = size / (c.page_size + sizeof(Page)) - 1;
  auto pages = align<Page>(num_pages, mem, size);
  auto pages_base = std::align(c.page_size, c.page_size * num_pages, mem, size);

  detail::construct(cxt, pages, num_pages, c.page_size, pages_base);
  detail::construct(page_alloc, pages, num_pages);

  for (int i = 0; i < num_heaps; i++) {
    detail::construct(heaps + i, std::ref(*cxt), std::ref(*page_alloc));
  }

  return detail::construct(imp, std::ref(*cxt), heaps, num_heaps, tcache,
                           max_threads);
}

static inline std::size_t tid_hash() {
  return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

void *Sheap::alloc(int tid, std::size_t size) noexcept {
  BOOST_ASSERT(size <= Bins.back());

  auto binid = BinMap[size];
  auto &heap = m_imp->m_heaps[tid & (m_imp->m_num_heaps - 1)];
  auto &tcache = m_imp->m_tcache[tid & (m_imp->m_max_threads - 1)][binid];

  return tcache.alloc(
      [&]() { return heap.alloc_pages(binid); },
      [&](auto &&_1) { return heap.push_full_pages(binid, _1); });
}

void *Sheap::alloc(std::size_t size) noexcept {
  return alloc(static_cast<int>(tid_hash()), size);
}

void Sheap::free(void *ptr) noexcept {
  BOOST_ASSERT(ptr != nullptr);
  auto page = m_imp->m_cxt.get_page(ptr);
  auto heap = page->get_heap();
  auto binid = page->get_size_class().binid;

  heap->deferred_free(binid, ptr);
}

void Sheap::collect_garbage(int opts) noexcept {
  auto flush_cache = (opts & sheap::flush_cache<true>::value) != 0;
  if (opts & collect_all<true>::value) {
    for (auto *heap = m_imp->m_heaps, *end = heap + m_imp->m_num_heaps;
         heap != end; heap++) {
      heap->collect_garbage(flush_cache);
    }
  } else {
    auto tid = tid_hash();
    auto &heap = m_imp->m_heaps[tid & (m_imp->m_num_heaps - 1)];
    heap.collect_garbage(flush_cache);
  }
}

} // namespace sheap
