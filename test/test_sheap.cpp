#include "sheap/Sheap.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <memory>
#include <thread>
#include <vector>

TEST_SUITE_BEGIN("sheap");

template <std::size_t Size> auto mem_alloc() {
  auto mem = std::make_unique<std::array<char, Size>>();

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

  std::thread{[&sheap]() {
    // Test deferred_free again
    for (auto i = 0; i < HUGE_ALLOC; i++) {
      if (!sheap.construct<int>(1, 0x7F7F7F7F))
        break;
    }
  }}.join();
}

TEST_SUITE_END();