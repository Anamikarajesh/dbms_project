# Experimental Performance Optimizations

Research findings and experimental implementations for maximum B+ tree performance.

## Performance Comparison

### Main vs Experimental

| Metric | Main (Binary) | Experimental (SIMD) | Improvement |
|--------|---------------|---------------------|-------------|
| Search (510 keys) | 36 ns | 23 ns | **35% faster** |
| Search (256 keys) | 32 ns | 16 ns | **50% faster** |
| Search (64 keys) | 19 ns | 6 ns | **68% faster** |
| Prefetch gain | 1.1x | 1.9x | **Better locality** |

### Current B+ Tree Benchmark

```
100K records:
├─ Insert:  592K ops/sec
├─ Read:    43M ops/sec
└─ Range:   0.22ms (10K results)
```

### With SIMD (Projected)

```
100K records (estimated):
├─ Insert:  ~600K ops/sec  (same)
├─ Read:    ~58M ops/sec   (+35%)
└─ Range:   ~0.18ms        (+20%)
```

---

## Promising Optimizations Found

### 1. SIMD-Accelerated Search (frozenca/BTree)
- **AVX-512** can search 16 int32 keys per instruction
- Potential **8-16x speedup** for internal node search
- Requires `-mavx512f` compiler flag

### 2. HugePages (TLB Optimization)
- 2MB pages vs 4KB = **512x fewer TLB entries**
- Eliminates TLB misses for hot index data
- Requires `mmap(..., MAP_HUGETLB, ...)`

### 3. van Emde Boas Layout
- Cache-oblivious tree layout
- Optimal for any cache hierarchy
- O(log_B N) memory transfers

### 4. Adaptive Radix Tree (ART)
- No rebalancing overhead
- O(k) lookup (k = key length)
- Better cache behavior than B+ trees

### 5. LMDB Techniques
- Append-only copy-on-write
- Zero-copy reads
- Lock-free readers

---

## Files in This Directory

| File | Description |
|------|-------------|
| `bptree_simd.hpp` | SIMD-accelerated search (AVX2) |
| `bptree_hugepages.hpp` | HugePages mmap (2MB pages) |
| `bptree_mlock.hpp` | Memory pinning (mlock) |
| `build_pgo.sh` | Profile-Guided Optimization script |
| `benchmark_advanced.cpp` | Comparative benchmarks |
| `research.md` | Detailed research notes |
| `results.md` | Experimental results |

---

## Quick Test

```bash
# SIMD benchmark
make && ./benchmark_advanced

# PGO build (5-15% improvement)
./build_pgo.sh
./bptree_pgo --benchmark
```

---

## References

- frozenca/BTree: github.com/frozenca/BTree
- LMDB: github.com/LMDB/lmdb
- ART: TUM paper "The Adaptive Radix Tree"
- HugePages: Linux kernel docs
