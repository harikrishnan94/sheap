#pragma once

#include <array>
#include <boost/align/align_down.hpp>

namespace sheap::detail {
struct SizeClass {
  int binid;
  int objsize;
  std::size_t page_size;
  std::size_t num_objs;

  constexpr SizeClass(int binid, int objsize, std::size_t page_size)
      : binid(binid), objsize(objsize), page_size(page_size),
        num_objs(page_size / objsize) {}

  SizeClass() = default;
};

constexpr auto MinAllocSize = 16;
constexpr auto MaxAllocSize = 4096;
constexpr auto InternalFragmentationLimit = 0.05;

static constexpr std::size_t get_num_bins() {
  auto num_bins = 0;
  auto distance = 16;
  auto alloc_size = 0;
  int frag_mul = 1 / InternalFragmentationLimit;

  while (alloc_size < MaxAllocSize) {
    auto next_distance = distance * 2;
    auto next_alloc_size = std::min<int>(
        boost::alignment::align_down(
            alloc_size + frag_mul * next_distance - distance, next_distance),
        MaxAllocSize);
    num_bins += (next_alloc_size - alloc_size) / distance;
    distance = next_distance;
    alloc_size = next_alloc_size;
  }

  return num_bins;
}

constexpr auto Bins = []() {
  std::array<int, get_num_bins()> bins{};
  auto distance = 16;
  auto alloc_size = 0;
  auto binid = 0;
  int frag_mul = 1 / InternalFragmentationLimit;

  while (alloc_size < MaxAllocSize) {
    auto next_distance = distance * 2;
    auto next_alloc_size = std::min<int>(
        boost::alignment::align_down(
            alloc_size + frag_mul * next_distance - distance, next_distance),
        MaxAllocSize);

    for (auto size = alloc_size + distance; size <= next_alloc_size;
         size += distance) {
      bins[binid++] = size;
    }

    distance = next_distance;
    alloc_size = next_alloc_size;
  }

  return bins;
}();

constexpr int NUM_BINS = Bins.size();
constexpr auto BinMap = []() {
  std::array<int, MaxAllocSize + 1> bmap{};

  for (int i = 0, binId = 0; i <= MaxAllocSize; i++) {
    if (Bins[binId] >= i) {
      bmap[i] = binId;
      continue;
    }
    binId++;
    bmap[i] = binId;
  }

  return bmap;
}();
} // namespace sheap::detail