#include "sheap/Sheap.h"
#include "sheap/detail/Heap.h"
#include "sheap/detail/ThreadCache.h"

#include <cstdio>
#include <memory>
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
static T *alloc_internal(std::size_t count, void *&mem, std::size_t &space) {
  if (std::align(alignof(T), sizeof(T) * count, mem, space)) {
    auto res = static_cast<T *>(mem);
    asan_unpoison_memory_region(res, sizeof(T) * count);
    mem = static_cast<void *>((static_cast<char *>(mem) + sizeof(T) * count));
    space -= sizeof(T) * count;
    return res;
  }

  throw std::bad_alloc{};
}

static inline ThreadCache **alloc_tcache(void *&mem, std::size_t &size,
                                         int max_threads) {
  auto tcache = alloc_internal<ThreadCache *>(max_threads, mem, size);

  for (int i = 0; i < max_threads; i++) {
    tcache[i] = alloc_internal<ThreadCache>(NUM_BINS, mem, size);

    for (int j = 0; j < NUM_BINS; j++)
      detail::construct(&tcache[i][j]);
  }

  return tcache;
}

Sheap::Sheap(void *mem, std::size_t size, const config &c)
    : m_imp(create(mem, size, c)) {}

Sheap::impl *Sheap::create(void *mem, std::size_t size, const config &c) {
  BOOST_ASSERT(c.max_threads > 0);

  asan_poison_memory_region(mem, size);

  auto num_heaps = detail::next_pow_2(c.num_heaps);
  auto max_threads = detail::next_pow_2(c.max_threads);

  auto imp = alloc_internal<impl>(1, mem, size);
  auto cxt = alloc_internal<Context>(1, mem, size);
  auto page_alloc = alloc_internal<PageAllocator>(1, mem, size);
  auto heaps = alloc_internal<Heap>(num_heaps, mem, size);
  auto tcache = alloc_tcache(mem, size, max_threads);
  auto num_pages = size / (c.page_size + sizeof(Page)) - 1;
  auto pages = alloc_internal<Page>(num_pages, mem, size);
  auto pages_base = std::align(c.page_size, c.page_size * num_pages, mem, size);

  detail::construct(cxt, pages, num_pages, c.page_size, pages_base);
  detail::construct(page_alloc, pages, num_pages);

  for (int i = 0; i < num_heaps; i++) {
    detail::construct(heaps + i, std::ref(*cxt), std::ref(*page_alloc));
  }

  return detail::construct(imp, std::ref(*cxt), heaps, num_heaps, tcache,
                           max_threads);
}

template <bool IsAlignedAlloc>
void *Sheap::alloc(int tid, std::size_t size) noexcept {
  BOOST_ASSERT(size <= max_alloc_size());

  auto binid = BinMap[size];
  auto &heap = m_imp->m_heaps[tid & (m_imp->m_num_heaps - 1)];
  auto &tcache = m_imp->m_tcache[tid & (m_imp->m_max_threads - 1)][binid];

  auto ret = tcache.alloc<IsAlignedAlloc>(
      [&]() { return heap.alloc_pages(binid); },
      [&](auto &&_1) { return heap.push_full_pages(binid, _1); });
  asan_unpoison_memory_region(ret, size);
  return ret;
}

void *Sheap::alloc(int tid, std::size_t size) noexcept {
  return alloc<false>(tid, size);
}

void *Sheap::aligned_alloc(int tid, std::size_t size,
                           std::size_t align) noexcept {
  auto binid = detail::BinMap[size];
  if (static_cast<std::size_t>(detail::Bins[binid].alignment) >= align) {
    return alloc<false>(tid, size);
  } else {
    auto unaligned = alloc<true>(tid, size + align - 1);
    auto aligned = boost::alignment::align_up(unaligned, align);
    auto in_acccessible = to_int(aligned) - to_int(unaligned);

    asan_poison_memory_region(unaligned, in_acccessible);
    return aligned;
  }
}

void Sheap::free(void *ptr) noexcept {
  BOOST_ASSERT(ptr != nullptr);
  auto [obj, page, szc] = m_imp->m_cxt.get_alloc_info(ptr);
  auto heap = page->get_heap();
  auto binid = szc.binid;

  asan_poison_memory_region(obj, szc.bin.size);
  heap->deferred_free(binid, obj);
}

void Sheap::collect_garbage(int tid, bool flush_cache) noexcept {
  if (tid < 0) {
    for (auto heap = m_imp->m_heaps, end = heap + m_imp->m_num_heaps;
         heap != end; heap++) {
      heap->collect_garbage(flush_cache);
    }
  } else {
    auto &heap = m_imp->m_heaps[tid & (m_imp->m_num_heaps - 1)];
    heap.collect_garbage(flush_cache);
  }
}

} // namespace sheap
