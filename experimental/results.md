# Experimental Results

## Test Environment
- Ubuntu Linux
- g++ with AVX2 support
- Compiled with `-O3 -march=native -mavx2`

---

## Search Algorithm Comparison

| Array Size | Linear | Binary | SIMD (AVX2) |
|------------|--------|--------|-------------|
| 16 keys | 8.6 ns | 8.3 ns (1.0x) | **3.9 ns (2.2x)** |
| 64 keys | 23.6 ns | 19.3 ns (1.2x) | **6.7 ns (3.5x)** |
| 256 keys | 54.5 ns | 32.2 ns (1.7x) | **21.2 ns (2.6x)** |
| **510 keys** | 87.1 ns | 33.4 ns (2.6x) | **24.6 ns (3.5x)** |
| 1024 keys | 137.4 ns | 59.8 ns (2.3x) | 60.9 ns (2.3x) |

### Key Findings

**For 510 keys (internal node size):**
- Linear search: 87.1 ns
- Binary search: 33.4 ns (**2.6x faster**)
- SIMD search: 24.6 ns (**3.5x faster**)

**SIMD provides 35% improvement over binary search** at our node size.

---

## Prefetch Effectiveness

Random access (64MB data, 4KB stride):
- Without prefetch: 333 µs
- With prefetch: 302 µs (**1.1x faster**)

---

## Recommendations

### Integrate into Main Code

1. **SIMD search** for internal nodes at 510 keys gives **3.5x speedup**
2. Current binary search is 2.6x faster than linear - good fallback
3. Prefetching provides modest but consistent gains

### Potential Improvements

| Optimization | Expected Gain | Effort |
|--------------|---------------|--------|
| SIMD search | 35% faster reads | Medium |
| HugePages | 10-20% faster | Low (config) |
| AVX-512 (if available) | 50%+ faster | Medium |

---

## How to Apply SIMD to Main Code

Replace `findChildIndex` in `page.hpp`:

```cpp
#include "experimental/bptree_simd.hpp"

FORCE_INLINE uint32_t findChildIndex(int32_t key) const {
    // Use SIMD for contiguous key arrays
    return experimental::simd_search_avx2(keys(), numKeys, key);
}
```

**Note:** Current layout is interleaved (child, key, child, key...).
Would need to change to separate key array for SIMD to work optimally.
