# bptree - Disk-Based B+ Tree Index

```
     ____  ____  _
    | __ )|  _ \| |_ _ __ ___  ___
    |  _ \| |_) | __| '__/ _ \/ _ \
    | |_) |  __/| |_| | |  __/  __/
    |____/|_|    \__|_|  \___|\___|

    High-Performance Database Indexing
```

A fast, memory-mapped B+ tree implementation for the DBMS course assignment.

## Quick Start

```bash
make
./bptree_driver
./bptree_driver --benchmark
```

## Requirements

```bash
sudo apt install g++ make
```

## Runtime Performance (100K Records)

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

## API

```cpp
bool writeData(int32_t key, const uint8_t* data);  // Insert/update
const uint8_t* readData(int32_t key);              // Point lookup
bool deleteData(int32_t key);                      // Remove
vector<uint8_t*> readRangeData(int32_t lo, int32_t hi, uint32_t& n);  // Range
```

## Project Structure

```
├── src/
│   ├── bptree.hpp        # B+ tree implementation
│   ├── page.hpp          # Page structures (SIMD optimized)
│   ├── page_manager.hpp  # mmap wrapper
│   └── driver.cpp        # Tests & benchmarks
├── report/
│   ├── approach.md
│   ├── analysis.md
│   └── conclusion.md
├── Makefile
└── README.md
```

## Optimizations

- AVX2 SIMD leaf search (8 keys/instruction)
- Binary search internal nodes (9 vs 510 comparisons)
- Prefetching during traversal
- `-O3 -march=native -flto`

## Specifications

- **Page size**: 4096 bytes
- **Leaf capacity**: 39 tuples
- **Internal fanout**: 510 children
- **Tree height**: ~3 for 1M records

---

*DBMS Assignment 2025*
