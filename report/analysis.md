# Performance Analysis

## Complexity

All operations are O(log n) in tree height:

| Operation | Time | Notes |
|-----------|------|-------|
| Insert | O(log n) | May trigger node splits |
| Delete | O(log n) | May trigger merges |
| Search | O(log n) | Single traversal |
| Range | O(log n + k) | k = result count |

## Tree Structure

With 4KB pages:

```
Records     Leaves    Height    I/O per lookup
─────────────────────────────────────────────
1,000       26        2         2
10,000      257       2         2
100,000     2,565     3         3
1,000,000   25,642    3         3
```

Even with 1M records, we only need 3 disk reads.

## Why mmap Works

Traditional I/O:
```
read(fd, buf, 4096);  // Syscall
memcpy(dest, buf);    // Extra copy
```

Memory-mapped I/O:
```
data[offset]  // Direct access, kernel handles paging
```

Benefits:
1. No explicit syscalls per page
2. OS page cache keeps hot pages in RAM
3. Lazy loading via page faults
4. Writes coalesced before disk sync

## Optimizations Applied

### Binary Search
```
Linear:   510 comparisons (worst case)
Binary:   9 comparisons (log₂ 510)
```

### Prefetching
```cpp
// While processing node N, prefetch node N+1
__builtin_prefetch(next_ptr, 0, 3);
```

### Compiler Flags
```
-O3                 # Aggressive optimization
-march=native       # CPU-specific SIMD
-flto               # Cross-file optimization
-ftree-vectorize    # Auto SIMD
-fno-exceptions     # Skip exception tables
```

### Kernel Hints
```cpp
madvise(ptr, size, MADV_RANDOM);    // Random access
madvise(ptr, size, MADV_WILLNEED);  // Prefetch
```

## Benchmark Results

After optimizations:

```
┌────────────┬─────────────────┬────────────────┬───────────┐
│   Scale    │     Insert      │      Read      │   Range   │
├────────────┼─────────────────┼────────────────┼───────────┤
│  1K keys   │  6.2M ops/sec   │  111M ops/sec  │  <0.01ms  │
│ 10K keys   │  4.1M ops/sec   │   81M ops/sec  │   0.01ms  │
│ 100K keys  │  766K ops/sec   │   40M ops/sec  │   0.33ms  │
└────────────┴─────────────────┴────────────────┴───────────┘
```

Key observations:
- Read throughput scales well (O(log n) tree height)
- Insert slows at 100K due to more node splits
- Range queries remain fast due to linked leaves
