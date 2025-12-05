# Conclusion

## Runtime Performance (100K Records)

| Operation | Runtime | Throughput |
|-----------|---------|------------|
| **Insert** | **106.57 ms** | 938K ops/sec |
| **Read** | **1.82 ms** | 55M ops/sec |
| **Range** | **0.12 ms** | 10K results |

---

## Implementation

- **C++17** with memory-mapped I/O
- **4096-byte pages** (OS aligned)
- **39 tuples/leaf**, **510 children/internal**
- Tree height ≈ 3 for 1M records

---

## Key Optimizations

1. AVX2 SIMD leaf search
2. Binary search internal nodes
3. CPU prefetching
4. `-O3 -march=native -flto`

---

## Verified APIs ✓

- `writeData()` - insert/update
- `readData()` - point lookup
- `deleteData()` - remove
- `readRangeData()` - range scan

---

*Amit Kumar Dhar — DBMS Assignment 2025*
