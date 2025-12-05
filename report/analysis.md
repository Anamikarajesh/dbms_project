# Performance Analysis

## Runtime Results

### Final Benchmark (100K Records)

| Operation | Runtime | Throughput |
|-----------|---------|------------|
| **Insert** | **106.57 ms** | 938K ops/sec |
| **Read** | **1.82 ms** | 55M ops/sec |
| **Range** | **0.12 ms** | 10K results |

### All Scales

```
┌──────────────┬─────────────┬────────────┬───────────┐
│    Scale     │   Insert    │    Read    │   Range   │
├──────────────┼─────────────┼────────────┼───────────┤
│   1K keys    │   0.24 ms   │   0.01 ms  │  <0.01 ms │
│  10K keys    │   2.47 ms   │   0.11 ms  │   0.01 ms │
│ 100K keys    │  106.57 ms  │   1.82 ms  │   0.12 ms │
└──────────────┴─────────────┴────────────┴───────────┘
```

---

## Complexity

| Operation | Complexity |
|-----------|------------|
| Insert | O(log n) |
| Delete | O(log n) |
| Search | O(log n) |
| Range | O(log n + k) |

---

## Optimizations

1. **AVX2 SIMD** - 8 key comparisons per instruction
2. **Binary search** - 9 comparisons for 510 keys
3. **mmap** - Zero-copy disk access
4. **Prefetching** - Hide memory latency
5. **Compiler flags** - `-O3 -march=native -flto`

---

## Tree Structure

- **Page size**: 4096 bytes
- **Leaf capacity**: 39 tuples
- **Internal fanout**: 510 children
- **Height for 1M records**: 3 levels
