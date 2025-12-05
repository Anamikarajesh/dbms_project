# Performance Research Notes

## 1. SIMD-Accelerated B+ Trees

### frozenca/BTree (AVX-512)
- Modern C++ header-only library
- Uses AVX-512 for 16-way parallel comparison
- Disk-backed via mmap supported

Key technique:
```cpp
__m512i keys = _mm512_loadu_si512(node->keys);
__m512i target = _mm512_set1_epi32(search_key);
__mmask16 mask = _mm512_cmplt_epi32_mask(target, keys);
int index = __builtin_ctz(mask);
```

Expected speedup: **10-20x** for internal node search

### Intel Palm Tree
- SIMD for unsorted nodes
- Linear SIMD scan beats binary search for small n
- Reduces branch mispredictions

---

## 2. HugePages Optimization

### Why It Helps
- Standard pages: 4KB → TLB covers 512KB (128 entries)
- Huge pages: 2MB → TLB covers 256MB (128 entries)
- **512x more memory addressable without TLB miss**

### Implementation
```cpp
#include <sys/mman.h>

// Option 1: Explicit hugepages
void* ptr = mmap(nullptr, size,
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
    -1, 0);

// Option 2: Transparent hugepages hint
madvise(ptr, size, MADV_HUGEPAGE);
```

### Requirements
```bash
# Check available hugepages
cat /proc/meminfo | grep Huge

# Allocate hugepages (as root)
echo 64 > /proc/sys/vm/nr_hugepages
```

---

## 3. Cache-Oblivious Layout

### van Emde Boas Layout
Recursive tree storage:
1. Split tree at height h/2
2. Store top subtree first
3. Recursively store bottom subtrees

Result: `O(log_B N)` memory transfers for ANY block size B

### Why It's Fast
- No need to tune for specific cache
- Works across L1, L2, L3, and disk
- 2-4x improvement in practice

---

## 4. Adaptive Radix Tree (ART)

### Why Consider ART
- Lookup: O(key_length) not O(log n)
- No rebalancing overhead
- Better cache locality than B+ trees
- 2.8-6x faster than B+ tree (USENIX paper)

### Trade-offs
- Complex implementation
- память Variable-length keys only
- Not standard for disk storage

---

## 5. LMDB Design Principles

### Key Features
1. **Single-level store**: No buffer manager
2. **Append-only COW**: No in-place updates
3. **Zero-copy reads**: Return pointers to mmap'd data
4. **Lock-free readers**: MVCC without locks

### Performance Claims
- 5-20x faster reads than Berkeley DB
- 10-25x faster writes than SQLite3

### Applicable Ideas
- Our current design already uses mmap
- Could add: lock-free read path
- Could add: copy-on-write for crash safety

---

## 6. Compiler and Hardware Optimizations

### Additional Flags to Try
```bash
-mavx2           # 256-bit SIMD
-mavx512f        # 512-bit SIMD
-mprefer-vector-width=512
-fprofile-generate / -fprofile-use  # PGO
```

### CPU Features to Exploit
- **CMOV**: Branchless conditionals
- **PREFETCHT0**: L1 cache prefetch
- **POPCNT/LZCNT**: Fast bit operations

---

## Recommendations

### Priority Order

1. **SIMD search** - Biggest gain, lowest risk
2. **Hugepages** - Free performance if available
3. **PGO compilation** - 10-20% improvement typical
4. **Cache-oblivious layout** - Significant rewrite needed
5. **ART** - Complete redesign, but fastest for pure in-memory

### For This Assignment

Given time constraints, focus on:
1. ✅ Binary search (done)
2. ✅ Prefetching (done)
3. ✅ SIMD search (experimental - 35% faster)
4. ✅ PGO compilation (experimental - 67% faster reads)
5. ⬜ HugePages (requires root)
6. ⬜ mlock memory pinning (requires root)

---

## 7. Profile-Guided Optimization (PGO)

### Results

```
With PGO (100K records):
├─ Insert:  511K ops/sec
├─ Read:    29M ops/sec
└─ Range:   0.32ms
```

**67% faster reads at 10K records** (83M vs 50M ops/sec)

### How to Use

```bash
cd experimental
./build_pgo.sh
./bptree_pgo --benchmark
```

---

## 8. Memory Locking (mlock)

Pins memory in RAM, preventing page faults.

### Benefits
- Zero swap latency
- Predictable performance
- Eliminates soft page faults

### Usage
Requires root or `CAP_IPC_LOCK` capability:
```bash
sudo ./your_program
```
