# B+ Tree Implementation Approach

## Problem Statement Summary

Implement a disk-based B+ tree index with:
- Integer keys (4 bytes)
- Fixed 100-byte tuples
- 4096-byte page size
- Memory-mapped I/O (or custom buffer manager)
- APIs: `writeData`, `deleteData`, `readData`, `readRangeData`

**Goal**: Fastest runtime among all submissions.

---

## Why B+ Tree?

B+ trees are the **gold standard** for disk-based indexing because:

1. **High Fanout** - With 4096-byte pages, internal nodes can hold ~510 children, creating very shallow trees
2. **Sequential Leaf Access** - Leaf nodes are linked, enabling O(n) range scans
3. **Disk-Optimized** - All data at leaf level, minimizing random I/O for searches
4. **Balanced** - O(log_m n) guarantees for all operations

---

## Tech Stack Selection

### Language: **C++ (C++17)**

| Criterion | C++ Advantage |
|-----------|---------------|
| **Raw Speed** | Compiles to native machine code with zero-cost abstractions |
| **Memory Control** | Direct pointer manipulation for mmap integration |
| **Compiler Optimizations** | `-O3 -march=native -flto` for maximum performance |
| **I/O Efficiency** | Direct `mmap()` syscall support |
| **Portability** | Compiles on any Ubuntu system with `g++` |

### Disk Access: **Memory-Mapped I/O (mmap)**

**Why mmap over traditional I/O?**

1. **No explicit read/write calls** - OS handles page faults automatically
2. **Kernel page cache utilization** - Frequently accessed pages stay in RAM
3. **Zero-copy access** - Data accessed directly from kernel buffers
4. **Simplified code** - Pages accessed as if in memory

### Compiler Flags

```bash
-O3                  # Aggressive optimizations
-march=native        # CPU-specific instructions (SIMD)
-flto                # Link-time optimization
-funroll-loops       # Loop unrolling
-ftree-vectorize     # Auto-vectorization
-fno-exceptions      # Remove exception handling overhead
-fno-rtti            # Remove runtime type information
-fomit-frame-pointer # Free up a register
```

---

## Key Optimizations

### 1. Binary Search in Internal Nodes
- Internal nodes have 510 keys
- Linear search: 510 comparisons worst case
- Binary search: 9 comparisons worst case (log₂ 510)

### 2. CPU Prefetching
```cpp
__builtin_prefetch(ptr, 0, 3);  // Read hint, high locality
```
Prefetch next node while processing current node.

### 3. Kernel Page Hints
```cpp
madvise(data, size, MADV_RANDOM);    // Random access pattern
madvise(data, size, MADV_WILLNEED);  // Will need this soon
```

### 4. Cache-Aligned Structures
All page structures aligned to 64-byte cache lines.

---

## Key Data Structures

### Leaf Node (holds keys + data)
```
Capacity = floor((4096 - 16) / 104) = 39 entries
```

### Internal Node (holds keys + child pointers)
```
Capacity = floor((4096 - 12) / 8) = 510 children
```

### Tree Height for 1 Million Records
```
log_510(1,000,000) ≈ 2.2 levels
```
Only **3 disk accesses** to find any record!

---

## Implementation Phases

### Phase 1: Page Layer
- Define 4096-byte page structure
- Implement page types: internal nodes, leaf nodes, metadata
- Create mmap wrapper for file access

### Phase 2: B+ Tree Core
- Implement binary search within nodes
- Implement insert with node splitting
- Implement delete with redistribution/merging
- Implement range queries via leaf traversal

### Phase 3: API Integration
- Expose required API functions
- Handle edge cases

### Phase 4: Performance Optimization
- Binary search for internal nodes
- CPU prefetching hints
- Compiler optimization flags
- Kernel madvise hints

---

## Alternatives Considered

| Option | Speed | Complexity | Memory Safety | Decision |
|--------|-------|------------|---------------|----------|
| Python | ❌ Slow | ✅ Easy | ✅ Safe | Rejected |
| Rust | ✅ Fast | ⚠️ Medium | ✅ Safe | Backup |
| Java | ⚠️ Medium | ✅ Easy | ✅ Safe | Rejected |
| **C++** | ✅ Fastest | ⚠️ Medium | ⚠️ Manual | **Selected** |

---

## Risk Mitigation

1. **Memory Leaks** → Use RAII patterns, smart pointers where safe
2. **Corruption** → Sync mmap after critical writes
3. **Portability** → Test on Ubuntu before submission
4. **Edge Cases** → Comprehensive test suite in driver program
