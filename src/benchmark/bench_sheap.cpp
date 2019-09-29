#include "sheap/Sheap.h"

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <iterator>
#include <random>
#include <vector>

class MallocAllocator {
public:
  void *alloc(int, std::size_t size) { return std::malloc(size); }
  void free(void *ptr) { std::free(ptr); }
};

class SheapAllocator {
public:
  SheapAllocator()
      : mem(std::malloc(MAX_MEMORY)), sheap(mem, MAX_MEMORY, config) {}

  ~SheapAllocator() { std::free(mem); }

  void *alloc(int tid, std::size_t size) { return sheap.alloc(tid, size); }
  void free(void *ptr) { sheap.free(ptr); }

  static SheapAllocator &instance() {
    static auto Instance = std::make_unique<SheapAllocator>();
    return *Instance;
  }

private:
  static constexpr auto MAX_MEMORY = 300'000'000;
  static inline auto config = sheap::config{
      static_cast<int>(std::thread::hardware_concurrency()), 64 * 1024, 1};

  void *mem;
  sheap::Sheap sheap;
};

template <typename Allocator>
static void BM_AllocFree(benchmark::State &s, Allocator &&a) {
  constexpr auto BATCH_SIZE = 100'000;
  constexpr auto MIN_ALLOCSIZE = 32;
  constexpr auto MAX_ALLOCSIZE = 4096;

  std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<std::int64_t> dist{MIN_ALLOCSIZE,
                                                   MAX_ALLOCSIZE};
  std::vector<std::int64_t> sizes;
  std::vector<void *> to_free;
  std::size_t block_size = s.range();

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

  auto tid = s.thread_index;
  while (s.KeepRunningBatch(BATCH_SIZE)) {
    prep_batch();
    for (auto i = 0; i < BATCH_SIZE; i++) {
      auto ptr = a.alloc(tid, sizes[i]);

      if (ptr == nullptr) {
        s.SkipWithError("OOM");
        break;
      }

      to_free.push_back(ptr);

      if (to_free.size() == block_size)
        free_all();
    }
  }

  free_all();
}

BENCHMARK_CAPTURE(BM_AllocFree, SheapAlloc, SheapAllocator::instance())
    ->ThreadRange(1, std::thread::hardware_concurrency())
    ->UseRealTime()
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2000)
    ->Arg(5000)
    ->Arg(10000);
BENCHMARK_CAPTURE(BM_AllocFree, Malloc, MallocAllocator{})
    ->ThreadRange(1, std::thread::hardware_concurrency())
    ->UseRealTime()
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2000)
    ->Arg(5000)
    ->Arg(10000);