#pragma once

#include <array>

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

constexpr std::array Bins = {
    32,   64,   80,   96,   112,  144,  160,  176,  192,  208,  224,
    240,  256,  272,  288,  304,  320,  336,  352,  368,  384,  400,
    416,  432,  448,  464,  480,  512,  544,  576,  608,  640,  672,
    704,  752,  784,  832,  880,  912,  960,  1024, 1088, 1168, 1232,
    1312, 1408, 1504, 1600, 1696, 1808, 1920, 2048, 2176, 2320, 2464,
    2608, 2752, 2896, 3072, 3264, 3472, 3696, 3888, 4096};

constexpr int NUM_BINS = Bins.size();
constexpr auto MAX_OBJECT_SIZE = Bins.back();
const auto BinMap = []() {
  std::array<int, MAX_OBJECT_SIZE + 1> bmap;

  for (int i = 0, binId = 0; i <= MAX_OBJECT_SIZE; i++) {
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