#include "sheap/Sheap.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <memory>
#include <random>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

TEST_SUITE_BEGIN("sheap");

static inline void clobber(void *mem, std::size_t size) {
  std::memset(mem, 0x7F, size);
}

template <std::size_t Size> auto mem_alloc() {
  auto mem = std::make_unique<std::array<char, Size>>();

  if constexpr (Size <= 10 * 1024 * 1024)
    std::fill(mem->begin(), mem->end(), 0x7F);

  return mem;
}

TEST_CASE("SheapBasic") {
  constexpr auto MAX_MEMORY = 1'000'000;
  constexpr auto NUM_ALLOC = 7001;
  constexpr auto HUGE_ALLOC = 20'000;
  constexpr auto INTVAL = 0xDEADBEF;
  auto mem = mem_alloc<MAX_MEMORY>();
  auto config = sheap::config{2, 64, 1};
  auto sheap = sheap::Sheap{mem.get(), MAX_MEMORY, config};
  std::vector<int *> ptrs;

  // Test Default allocation cases
  for (auto i = 0; i < NUM_ALLOC; i++) {
    auto ptr = sheap.construct<int>(0, INTVAL);
    REQUIRE(ptr != nullptr);
    REQUIRE(*ptr == INTVAL);
    ptrs.push_back(ptr);
  }

  // Test free
  for (auto ptr : ptrs) {
    sheap.free(ptr);
  }

  ptrs.clear();

  // Test get_partial_pages
  for (auto i = 0; i < NUM_ALLOC; i++) {
    auto ptr = sheap.construct<int>(0, INTVAL);
    REQUIRE(ptr != nullptr);
    REQUIRE(*ptr == INTVAL);
    ptrs.push_back(ptr);
  }

  for (auto ptr : ptrs) {
    sheap.free(ptr);
  }

  sheap.collect_garbage<sheap::flush_cache<true>>(0);

  std::thread{[&sheap]() {
    // Test deferred_free again
    for (auto i = 0; i < HUGE_ALLOC; i++) {
      if (!sheap.construct<int>(1, INTVAL))
        break;
    }
  }}.join();
  sheap.collect_garbage<sheap::flush_cache<true>>(1);
}

TEST_CASE("SheapRandom") {
  enum { ALLOC, FREE, GC };

  constexpr auto iterations = 100'000;
  constexpr auto ALLOCATOR_SIZE = 4'000'000'000UL;
  auto mem = mem_alloc<ALLOCATOR_SIZE>();
  constexpr auto NUM_THREADS = 8;
  auto config = sheap::config{NUM_THREADS, 64 * 1024, 1};
  auto sheap = sheap::Sheap{mem.get(), ALLOCATOR_SIZE, config};

  auto test = [&](int tid, auto ptrs) {
    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> size_dist{32, 4096};
    std::uniform_int_distribution<> op_dist{1, 10000};

    auto next_op = [&]() {
      auto val = op_dist(gen);

      if (val < 5000) {
        return ALLOC;
      } else if (val < 9999) {
        return FREE;
      } else {
        return GC;
      }
    };

    for (int i = 0; i < iterations;) {
      switch (next_op()) {
      case ALLOC: {
        auto size = size_dist(gen);
        auto ptr = sheap.alloc(tid, size);

        REQUIRE(ptr != nullptr);
        REQUIRE(ptrs.count(ptr) == 0);

        clobber(ptr, size);
        ptrs.insert(ptr);
        i++;
        break;
      }

      case FREE:
        if (auto it = ptrs.begin(); it != ptrs.end()) {
          auto mem = *it;
          ptrs.erase(it);
          sheap.free(mem);
        }

        break;

      case GC:
        sheap.collect_garbage_full();
        break;
      }
    }
  };

  std::vector<std::thread> workers;

  for (auto i = 0; i < NUM_THREADS; i++) {
    workers.emplace_back(
        [&](auto tid) {
          std::unordered_set<void *> ptrs;
          test(tid, ptrs);
          ptrs.clear();
          test(tid, ptrs);
        },
        i);
  }

  for (auto &t : workers) {
    t.join();
  }
}

TEST_SUITE_END();