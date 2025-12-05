#ifndef BPTREE_SIMD_HPP
#define BPTREE_SIMD_HPP

/**
 * EXPERIMENTAL: SIMD-Accelerated B+ Tree Search
 *
 * Uses AVX2 to compare 8 int32 keys per instruction.
 * Falls back to scalar binary search if AVX2 not available.
 */

#include <cstdint>
#include <cstring>

// Check for AVX2 support
#if defined(__AVX2__)
#include <immintrin.h>
#define HAVE_AVX2 1
#endif

// Force inline for hot paths
#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

namespace experimental {

/**
 * AVX2-accelerated search in sorted int32 array.
 * Returns index of first element >= key, or n if all < key.
 *
 * For 510 keys: ~64 iterations (510/8) with SIMD
 * vs ~9 iterations with binary search
 *
 * SIMD wins when:
 * - Keys are contiguous in memory (cache-friendly)
 * - Branch misprediction cost > SIMD instruction cost
 */
FORCE_INLINE uint32_t simd_search_avx2(const int32_t *keys, uint32_t n,
                                       int32_t target) {
#ifdef HAVE_AVX2
  if (n == 0)
    return 0;

  // Broadcast target to all 8 lanes
  __m256i target_vec = _mm256_set1_epi32(target);

  uint32_t i = 0;

  // Process 8 keys at a time
  for (; i + 8 <= n; i += 8) {
    // Load 8 keys (must be 32-byte aligned for best performance)
    __m256i keys_vec =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(keys + i));

    // Compare: target < keys[i]
    // Result: 0xFFFFFFFF if true, 0 if false
    __m256i cmp = _mm256_cmpgt_epi32(keys_vec, target_vec);

    // Convert to bitmask
    int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));

    if (mask != 0) {
      // Found first key > target
      return i + __builtin_ctz(mask);
    }
  }

  // Scalar cleanup for remaining keys
  for (; i < n; ++i) {
    if (keys[i] > target)
      return i;
  }

  return n;
#else
  // Fallback: binary search
  uint32_t lo = 0, hi = n;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (keys[mid] <= target)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
#endif
}

/**
 * AVX2-accelerated exact match search.
 * Returns index if found, or n if not found.
 */
FORCE_INLINE uint32_t simd_find_exact(const int32_t *keys, uint32_t n,
                                      int32_t target) {
#ifdef HAVE_AVX2
  __m256i target_vec = _mm256_set1_epi32(target);

  uint32_t i = 0;
  for (; i + 8 <= n; i += 8) {
    __m256i keys_vec =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(keys + i));
    __m256i cmp = _mm256_cmpeq_epi32(keys_vec, target_vec);
    int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));

    if (mask != 0) {
      return i + __builtin_ctz(mask);
    }
  }

  // Scalar cleanup
  for (; i < n; ++i) {
    if (keys[i] == target)
      return i;
  }

  return n; // Not found
#else
  // Fallback: binary search for sorted array
  uint32_t lo = 0, hi = n;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (keys[mid] < target)
      lo = mid + 1;
    else if (keys[mid] > target)
      hi = mid;
    else
      return mid; // Found
  }
  return n; // Not found
#endif
}

/**
 * Prefetch multiple cache lines ahead.
 * Useful for sequential scan operations.
 */
FORCE_INLINE void prefetch_ahead(const void *ptr, int cache_lines = 4) {
#if defined(__GNUC__) || defined(__clang__)
  for (int i = 0; i < cache_lines; ++i) {
    __builtin_prefetch(static_cast<const char *>(ptr) + i * 64, 0, 3);
  }
#endif
}

} // namespace experimental

#endif // BPTREE_SIMD_HPP
