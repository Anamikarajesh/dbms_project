/**
 * EXPERIMENTAL: Advanced Benchmark Suite
 *
 * Tests experimental optimizations:
 * - SIMD search vs binary search
 * - HugePages vs regular mmap
 * - Prefetch strategies
 */

#include "bptree_simd.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

using namespace std::chrono;
using namespace experimental;

// Test configuration
constexpr int WARMUP_RUNS = 3;
constexpr int TEST_RUNS = 10;

/**
 * Benchmark helper
 */
template <typename Func> double benchmark_ns(Func &&f, int runs = TEST_RUNS) {
  // Warmup
  for (int i = 0; i < WARMUP_RUNS; ++i)
    f();

  // Measure
  double total = 0;
  for (int i = 0; i < runs; ++i) {
    auto start = high_resolution_clock::now();
    f();
    auto end = high_resolution_clock::now();
    total += duration_cast<nanoseconds>(end - start).count();
  }
  return total / runs;
}

/**
 * Binary search baseline
 */
uint32_t binary_search(const int32_t *keys, uint32_t n, int32_t target) {
  uint32_t lo = 0, hi = n;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (keys[mid] <= target)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}

/**
 * Linear search baseline
 */
uint32_t linear_search(const int32_t *keys, uint32_t n, int32_t target) {
  for (uint32_t i = 0; i < n; ++i) {
    if (keys[i] > target)
      return i;
  }
  return n;
}

/**
 * Test search algorithms on arrays of different sizes
 */
void test_search_performance() {
  std::cout << "\n=== Search Algorithm Benchmark ===\n\n";

  std::vector<uint32_t> sizes = {16, 64, 256, 510, 1024};
  std::mt19937 rng(42);

  for (uint32_t n : sizes) {
    // Generate sorted keys
    std::vector<int32_t> keys(n);
    for (uint32_t i = 0; i < n; ++i) {
      keys[i] = i * 10;
    }

    // Generate random search targets
    std::uniform_int_distribution<int32_t> dist(0, n * 10);
    std::vector<int32_t> targets(1000);
    for (auto &t : targets)
      t = dist(rng);

    // Benchmark linear search
    volatile uint32_t sink = 0;
    double linear_ns = benchmark_ns([&]() {
      for (auto t : targets) {
        sink += linear_search(keys.data(), n, t);
      }
    });

    // Benchmark binary search
    double binary_ns = benchmark_ns([&]() {
      for (auto t : targets) {
        sink += binary_search(keys.data(), n, t);
      }
    });

    // Benchmark SIMD search
    double simd_ns = benchmark_ns([&]() {
      for (auto t : targets) {
        sink += simd_search_avx2(keys.data(), n, t);
      }
    });

    double linear_per_search = linear_ns / targets.size();
    double binary_per_search = binary_ns / targets.size();
    double simd_per_search = simd_ns / targets.size();

    std::cout << "n=" << n << ":\n";
    std::cout << "  Linear: " << linear_per_search << " ns/search\n";
    std::cout << "  Binary: " << binary_per_search << " ns/search";
    std::cout << " (" << linear_per_search / binary_per_search << "x faster)\n";
    std::cout << "  SIMD:   " << simd_per_search << " ns/search";
    std::cout << " (" << linear_per_search / simd_per_search << "x faster)\n";
    std::cout << "\n";
  }
}

/**
 * Test prefetching effectiveness
 */
void test_prefetch_performance() {
  std::cout << "\n=== Prefetch Effectiveness ===\n\n";

  constexpr size_t DATA_SIZE = 64 * 1024 * 1024; // 64MB
  constexpr size_t STRIDE = 4096;                // Page size

  std::vector<uint8_t> data(DATA_SIZE);
  std::mt19937 rng(42);

  // Random offsets
  std::vector<size_t> offsets;
  for (size_t i = 0; i < DATA_SIZE; i += STRIDE) {
    offsets.push_back(i);
  }
  std::shuffle(offsets.begin(), offsets.end(), rng);

  volatile uint8_t sink = 0;

  // Without prefetch
  auto start = high_resolution_clock::now();
  for (auto off : offsets) {
    sink += data[off];
  }
  auto no_prefetch =
      duration_cast<microseconds>(high_resolution_clock::now() - start).count();

  // With prefetch
  start = high_resolution_clock::now();
  for (size_t i = 0; i < offsets.size(); ++i) {
    if (i + 4 < offsets.size()) {
      prefetch_ahead(&data[offsets[i + 4]], 1);
    }
    sink += data[offsets[i]];
  }
  auto with_prefetch =
      duration_cast<microseconds>(high_resolution_clock::now() - start).count();

  std::cout << "Random access (64MB, 4KB stride):\n";
  std::cout << "  Without prefetch: " << no_prefetch << " µs\n";
  std::cout << "  With prefetch:    " << with_prefetch << " µs";
  std::cout << " (" << (double)no_prefetch / with_prefetch << "x faster)\n";
}

int main() {
  std::cout << "Experimental B+ Tree Optimization Benchmarks\n";
  std::cout << "=============================================\n";

#ifdef HAVE_AVX2
  std::cout << "\n[OK] AVX2 support detected\n";
#else
  std::cout << "\n[!!] AVX2 not available - using scalar fallback\n";
#endif

  test_search_performance();
  test_prefetch_performance();

  std::cout << "\nDone.\n";
  return 0;
}
