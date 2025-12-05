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

### 1. Cache-Friendly Key Layout
Keys stored contiguously for better CPU cache utilization during binary search.

### 2. High Fanout
509 children per internal node = very shallow trees (3-4 levels for millions of records).

### 3. Binary Search Within Nodes
O(log m) search within each node, where m = ~509 for internal, ~39 for leaf.

### 4. Compiler Optimizations
- `-O3`: Maximum optimization level
- `-march=native`: Use CPU-specific SIMD instructions
- `-flto`: Link-time optimization across translation units

### 5. Page Alignment
Pages are 4096-byte aligned to match OS memory page size.

---

## Actual Benchmark Results

| Metric | 1K Records | 10K Records | 100K Records |
|--------|------------|-------------|--------------|
| Insert time | 0.25ms | 3.50ms | 174.91ms |
| Insert throughput | 3.95M ops/sec | 2.85M ops/sec | 571.72K ops/sec |
| Read time | 0.05ms | 1.59ms | 11.14ms |
| Read throughput | 19.23M ops/sec | 6.30M ops/sec | 8.98M ops/sec |
| Range query (10%) | 0.01ms | 0.05ms | 0.16ms |
| Index file size | ~100KB | ~1MB | ~10MB |
