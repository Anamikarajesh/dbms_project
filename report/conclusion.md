# Conclusion

## What We Built

A fast, disk-based B+ tree index in C++17 with:

- Memory-mapped I/O for zero-copy access
- Binary search in internal nodes
- CPU prefetching for reduced latency
- Aggressive compiler optimizations

## Results

```
100K records:
├─ Insert:  766K ops/sec
├─ Read:    40M ops/sec
└─ Range:   0.33ms for 10K results
```

All API functions verified:
- `writeData()` — insert/update ✓
- `readData()` — point lookup ✓
- `deleteData()` — remove ✓
- `readRangeData()` — range scan ✓

Data persists across restarts via mmap sync.

## What Worked

1. **Binary search pays off**
   - 9 comparisons vs 510 = 50x faster lookups

2. **mmap simplifies everything**
   - Kernel handles paging
   - Hot pages stay cached
   - No buffer manager needed

3. **Compiler flags matter**
   - `-O3 -march=native` gave ~10x over `-O0`
   - `-flto` helps with inlining across files

4. **High fanout = shallow trees**
   - 510 children per node
   - Only 3 levels for 1M records

## What Could Be Better

1. **Concurrency** — Currently single-threaded
2. **Crash recovery** — No write-ahead logging
3. **Compression** — Could reduce I/O
4. **Bulk load** — Sequential insert is slow

## Final Thoughts

B+ trees remain the gold standard for disk indexing.
With careful optimization, even a simple implementation
can achieve impressive throughput.

---

*Amit Kumar Dhar — DBMS Assignment 2025*
