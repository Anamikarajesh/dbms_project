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

1. **High Fanout** - With 4096-byte pages, internal nodes can hold ~509 children, creating very shallow trees
2. **Sequential Leaf Access** - Leaf nodes are linked, enabling O(n) range scans
3. **Disk-Optimized** - All data at leaf level, minimizing random I/O for searches
4. **Balanced** - O(log_m n) guarantees for all operations

---

## Tech Stack Selection

### Language: **C++ (C++17)**

**Reasoning**:

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
-O3              # Aggressive optimizations
-march=native    # CPU-specific instructions
-flto            # Link-time optimization
-funroll-loops   # Loop unrolling
```

---

## Implementation Strategy

### Phase 1: Page Layer
- Define 4096-byte page structure
- Implement page types: internal nodes, leaf nodes, metadata
- Create mmap wrapper for file access

### Phase 2: B+ Tree Core
- Implement recursive search
- Implement insert with node splitting
- Implement delete with redistribution/merging
- Implement range queries via leaf traversal

### Phase 3: API Integration
- Expose required API functions
- Handle edge cases and special requirements

### Phase 4: Optimization
- Profile with `perf` or `valgrind`
- Optimize hot paths
- Reduce cache misses

---

## Key Data Structures

### Leaf Node (holds keys + data)
```
Capacity = floor((4096 - 16) / 104) = 39 entries
```

### Internal Node (holds keys + child pointers)
```
Capacity = floor((4096 - 16 - 4) / 8) = 509 children
```

### Tree Height for 1 Million Records
```
log_509(1,000,000) ≈ 2.2 levels
```
Only **3 disk accesses** to find any record!

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
