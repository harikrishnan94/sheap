#include "sheap/Sheap.h"

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <iterator>
#include <random>
#include <utility>
#include <vector>

template <typename T1, typename T2> constexpr auto alloc_range(T1 &&a, T2 &&b) {
  return std::make_pair(a, b);
}

static const int MAX_THREADS = std::thread::hardware_concurrency();
static constexpr auto MAX_LIVE_OBJECTS = 10000;

struct AllocRanges {
public:
  static constexpr int get_alloc_sizes_max_range_id() {
    return alloc_size_ranges.size() - 1;
  }
  static constexpr auto get_alloc_range(int rid) {
    return alloc_size_ranges.at(rid);
  }

private:
  static constexpr auto alloc_size_ranges = std::array{
      alloc_range(80, 512), alloc_range(512, 1024), alloc_range(1024, 4096)};
};

class MallocAllocator {
public:
  void *alloc(int, std::size_t size) { return std::malloc(size); }
  void free(void *ptr) { std::free(ptr); }

  static MallocAllocator &instance(int) {
    static auto Instance = std::make_unique<MallocAllocator>();
    return *Instance;
  }
};

class SheapAllocator {
public:
  SheapAllocator(int num_heaps)
      : mem(std::malloc(determine_memory())),
        sheap(mem, determine_memory(),
              {static_cast<int>(MAX_THREADS), 64 * 1024,
               static_cast<size_t>(num_heaps)}) {}

  SheapAllocator(const SheapAllocator &) = delete;

  SheapAllocator(SheapAllocator &&o)
      : mem(std::exchange(o.mem, nullptr)), sheap(std::move(o.sheap)) {}
  ~SheapAllocator() {
    if (mem)
      std::free(mem);
  }

  void *alloc(int tid, std::size_t size) { return sheap.alloc(tid, size); }
  void free(void *ptr) { sheap.free(ptr); }

  static SheapAllocator &instance(int num_heaps) {
    static auto sheap_allocators = []() {
      std::vector<SheapAllocator> sheaps;

      for (auto num_heaps = 0; num_heaps < MAX_THREADS; num_heaps++) {
        sheaps.emplace_back(num_heaps);
      }

      sheaps.shrink_to_fit();
      return sheaps;
    }();

    return sheap_allocators[num_heaps];
  }

private:
  static std::size_t determine_memory() {
    return (MAX_LIVE_OBJECTS + MAX_LIVE_OBJECTS / 10) * MAX_THREADS *
           sheap::Sheap::max_object_size();
  }
  void *mem;
  sheap::Sheap sheap;
};

template <typename Allocator> static void BM_AllocFree(benchmark::State &s) {
  auto alloc_range = s.range(0);
  auto min_allocsize = AllocRanges::get_alloc_range(alloc_range).first;
  auto max_allocsize = AllocRanges::get_alloc_range(alloc_range).second;
  auto block_size = s.range(1);
  auto num_heaps = s.range(2);

  std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dist(min_allocsize, max_allocsize);
  std::vector<std::size_t> sizes;
  std::vector<void *> to_free;
  std::int64_t total_size_alloc = 0;

  auto &a = Allocator::instance(num_heaps - 1);
  auto tid = s.thread_index;

  constexpr auto BATCH_SIZE = 100'000;
  auto prep_batch = [&]() {
    s.PauseTiming();
    sizes.clear();
    std::generate_n(std::back_inserter(sizes), BATCH_SIZE,
                    [&gen, &dist]() { return dist(gen); });
    s.ResumeTiming();
  };
  auto free_all = [&]() {
    for (auto p : to_free) {
      a.free(p);
    }

    to_free.clear();
  };

  while (s.KeepRunningBatch(BATCH_SIZE)) {
    prep_batch();
    for (auto i = 0; i < BATCH_SIZE; i++) {
      auto ptr = a.alloc(tid, sizes[i]);

      if (ptr == nullptr) {
        s.SkipWithError("OOM");
        break;
      }

      total_size_alloc += sizes[i];
      to_free.push_back(ptr);

      if (to_free.size() == static_cast<std::size_t>(block_size))
        free_all();
    }
  }

  free_all();
}

static void SheapAllocArgsGen(benchmark::internal::Benchmark *b) {
  for (auto alloc_range_id = 0;
       alloc_range_id <= AllocRanges::get_alloc_sizes_max_range_id();
       alloc_range_id++) {
    for (auto live_obj_count : {100, 500, 1000, 5000, 10000}) {
      for (auto num_heaps : {1, 2, 4, 8}) {
        b->Args({alloc_range_id, live_obj_count, num_heaps});
      }
    }
  }
}

static void MallocArgsGen(benchmark::internal::Benchmark *b) {
  for (auto alloc_range_id = 0;
       alloc_range_id <= AllocRanges::get_alloc_sizes_max_range_id();
       alloc_range_id++) {
    for (auto live_obj_count : {100, 500, 1000, 5000, 10000}) {
      b->Args({alloc_range_id, live_obj_count, 0});
    }
  }
}

BENCHMARK_TEMPLATE(BM_AllocFree, SheapAllocator)
    ->ThreadRange(1, MAX_THREADS)
    ->Apply(SheapAllocArgsGen);

BENCHMARK_TEMPLATE(BM_AllocFree, MallocAllocator)
    ->ThreadRange(1, MAX_THREADS)
    ->Apply(MallocArgsGen);