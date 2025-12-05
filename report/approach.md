# Design Approach

## The Problem

Build a disk-based B+ tree index that:
- Stores integer keys with 100-byte tuples
- Uses 4KB pages
- Supports insert, delete, point query, and range query
- Must be fast — marks depend on runtime

## Why B+ Trees?

Simple: they're the industry standard for disk-based indexing.

```
                    [Root]
                   /      \
            [Internal]  [Internal]
             /  |  \      /  |  \
          [L1][L2][L3] [L4][L5][L6]  ← Linked leaves
```

With 4KB pages:
- Internal nodes hold **510 children**
- Leaves hold **39 tuples**
- 1 million records fit in **3 tree levels**

That's just 3 disk reads to find any record.

## Tech Stack

**Language: C++17**

We need raw speed. C++ gives us:
- Zero-cost abstractions
- Direct memory control
- Native SIMD via `-march=native`

**Disk I/O: mmap**

Memory-mapped files let us treat the index like RAM:

```cpp
data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```

The kernel handles paging automatically. No explicit read/write calls.

## Key Design Decisions

### 1. Binary Search in Internal Nodes

Internal nodes have 510 keys. Linear search = 510 comparisons worst case.

With binary search: **9 comparisons max** (log₂ 510).

This is a 50x improvement for every tree traversal.

### 2. CPU Prefetching

Modern CPUs can fetch data ahead of time:

```cpp
__builtin_prefetch(next_node, 0, 3);  // Read, high locality
```

We prefetch the next node while processing the current one.
Hides memory latency.

### 3. Kernel Hints

Tell the OS about our access patterns:

```cpp
madvise(data, size, MADV_RANDOM);     // We jump around
madvise(data, size, MADV_WILLNEED);   // Prefetch these pages
```

### 4. Compiler Flags

```bash
-O3                  # Max optimization
-march=native        # Use CPU's SIMD
-flto                # Link-time optimization
-fno-exceptions      # Skip exception tables
-fomit-frame-pointer # Free up a register
```

## Implementation Phases

1. **Page layer** — Define 4KB structures, mmap wrapper
2. **Tree core** — Insert, delete, search with splitting
3. **API layer** — Expose writeData, readData, etc.
4. **Optimization** — Binary search, prefetch, compiler flags

## Alternatives Considered

| Language | Speed | Why not? |
|----------|-------|----------|
| Python | Slow | 100x slower than C++ |
| Java | Medium | JVM overhead |
| Rust | Fast | Higher complexity |

C++ wins for raw performance with mmap.

## Risk Mitigation

- **Memory safety:** RAII patterns, careful pointer handling
- **Data corruption:** msync after critical writes
- **Portability:** Tested on Ubuntu before submission
