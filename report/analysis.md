# Performance Analysis

## Theoretical Complexity

| Operation | Average Case | Worst Case | Notes |
|-----------|--------------|------------|-------|
| Insert | O(log n) | O(log n) | May trigger splits |
| Delete | O(log n) | O(log n) | May trigger merges |
| Search | O(log n) | O(log n) | Always balanced |
| Range Query | O(log n + k) | O(log n + k) | k = result size |

---

## Disk I/O Analysis

### Page Access Per Operation

| Operation | Best Case | Typical | Worst Case |
|-----------|-----------|---------|------------|
| Search | 1 (cached) | 2-3 | log_m(n) |
| Insert | 2-3 | 3-4 | 2*log_m(n) |
| Delete | 2-3 | 3-4 | 2*log_m(n) |
| Range(k) | log_m(n) | log_m(n) + k/39 | Full scan |

### Tree Height vs Data Size

| Records | Leaf Nodes | Tree Height | Max I/O Per Search |
|---------|------------|-------------|-------------------|
| 1,000 | 26 | 2 | 2 |
| 10,000 | 257 | 2 | 2 |
| 100,000 | 2,565 | 3 | 3 |
| 1,000,000 | 25,642 | 3 | 3 |
| 10,000,000 | 256,411 | 4 | 4 |

---

## Memory-Mapped I/O Benefits

1. **Reduced System Calls**: Traditional I/O requires explicit `read()`/`write()` syscalls for each page access
2. **OS Page Cache**: Hot pages automatically cached by kernel
3. **Lazy Loading**: Pages loaded on-demand via page faults
4. **Write Coalescing**: Multiple writes to same page merged before disk sync

---

## Optimization Techniques Applied

### 1. Binary Search for Internal Nodes
Replaced linear search with O(log m) binary search in internal nodes (510 keys).
- Linear: 510 comparisons worst case
- Binary: 9 comparisons worst case (log₂ 510)

### 2. CPU Prefetching
Used `__builtin_prefetch()` to hint CPU to load next node during tree traversal, hiding memory latency.

### 3. madvise Kernel Hints
- `MADV_RANDOM`: Optimized for random access patterns
- `MADV_WILLNEED`: Pre-fetch metadata and root pages

### 4. Aggressive Compiler Optimizations
```bash
-O3                 # Maximum optimization
-march=native       # CPU-specific SIMD
-flto               # Link-time optimization
-ftree-vectorize    # Auto-vectorization
-fno-exceptions     # Remove exception overhead
-fomit-frame-pointer # Free up register
```

### 5. Memory Layout
- Cache-aligned structures (64-byte alignment)
- Contiguous key arrays for cache locality
- Pre-allocated result vectors for range queries

---

## Benchmark Results (Optimized)

| Metric | 1K Records | 10K Records | 100K Records |
|--------|------------|-------------|--------------|
| Insert time | 0.16ms | 2.45ms | 130.62ms |
| Insert throughput | **6.2M ops/sec** | **4.1M ops/sec** | **766K ops/sec** |
| Read time | 0.01ms | 0.12ms | 2.49ms |
| Read throughput | **111M ops/sec** | **81M ops/sec** | **40M ops/sec** |
| Range query (10%) | 0.00ms | 0.01ms | 0.33ms |
