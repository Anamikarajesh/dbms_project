# B+ Tree Disk-Based Index

A high-performance B+ tree index implementation for database management systems.

## Features

- **Disk-based storage** using memory-mapped I/O (`mmap`)
- **4096-byte page size** optimized for OS page alignment
- **High fanout** (~510 children per internal node)
- **Binary search** in internal nodes (9 comparisons vs 510 linear)
- **CPU prefetching** for reduced memory latency
- **Range queries** via linked leaf nodes
- **Cross-platform** (Windows + Linux/Ubuntu)

## Prerequisites

### Ubuntu/Linux
```bash
sudo apt-get update
sudo apt-get install g++ make
```

### Windows
- MinGW-w64 with g++ (or MSVC)
- GNU Make (via MSYS2 or similar)

## Compilation

### Quick Start
```bash
# Build optimized release version
make

# Or explicitly
make release
```

### Build Targets
| Command | Description |
|---------|-------------|
| `make` | Build release version (default) |
| `make release` | Build with -O3 optimizations |
| `make debug` | Build with debug symbols |
| `make clean` | Remove build artifacts |

## Execution

### Run Tests
```bash
./bptree_driver
```

### Run Benchmarks
```bash
./bptree_driver --benchmark
```

## API Documentation

### writeData(key, data)
Inserts or updates a key-value pair in the index.

**Parameters:**
- `key` (int32_t): The integer key
- `data` (uint8_t*): Pointer to 100-byte tuple data

**Returns:** `true` (1) on success, `false` (0) on failure

**Example:**
```cpp
BPlusTree tree;
tree.open("index.idx");

uint8_t data[100] = {0};
// Fill data...
bool success = tree.writeData(42, data);
```

---

### deleteData(key)
Deletes a key from the index.

**Parameters:**
- `key` (int32_t): The key to delete

**Returns:** `true` (1) if deleted, `false` (0) if key not found

**Example:**
```cpp
bool deleted = tree.deleteData(42);
```

---

### readData(key)
Searches for a key and returns the associated data.

**Parameters:**
- `key` (int32_t): The key to search for

**Returns:** Pointer to 100-byte tuple data, or `NULL` (0) if not found

**Example:**
```cpp
const uint8_t* result = tree.readData(42);
if (result) {
    // Use data...
}
```

---

### readRangeData(lowerKey, upperKey, n)
Returns all tuples with keys in the range [lowerKey, upperKey].

**Parameters:**
- `lowerKey` (int32_t): Lower bound (inclusive)
- `upperKey` (int32_t): Upper bound (inclusive)
- `n` (uint32_t&): Output parameter for result count

**Returns:** Array of tuple pointers, or `NULL` (0) if no keys in range

**Example:**
```cpp
uint32_t count;
auto results = tree.readRangeData(10, 50, count);
for (uint32_t i = 0; i < count; i++) {
    // Process results[i]...
}
```

## File Structure

```
DBMS_PROJECT/
├── src/
│   ├── bptree.hpp       # B+ tree implementation
│   ├── page.hpp         # Page structures with binary search
│   ├── page_manager.hpp # Memory-mapped I/O with madvise
│   └── driver.cpp       # Test driver
├── logs/                # Test logs
├── report/              # Analysis reports
├── Makefile             # Build configuration
└── README.md            # This file
```

## Performance

| Operation | Complexity |
|-----------|------------|
| Insert | O(log n) |
| Delete | O(log n) |
| Search | O(log n) |
| Range | O(log n + k) |

### Benchmark Results (Optimized)

| Scale | Insert | Read | Range Query |
|-------|--------|------|-------------|
| 1K records | 6.2M ops/sec | 111M ops/sec | <0.01ms |
| 10K records | 4.1M ops/sec | 81M ops/sec | 0.01ms |
| 100K records | 766K ops/sec | 40M ops/sec | 0.33ms |

### Data Structure Capacities

With 4096-byte pages:
- **Leaf capacity**: 39 records
- **Internal capacity**: 510 children
- **Tree height for 1M records**: ~3 levels

## Optimizations

1. **Binary search** in internal nodes (log₂ 510 ≈ 9 comparisons)
2. **CPU prefetching** via `__builtin_prefetch()`
3. **madvise hints** for kernel page management
4. **Aggressive compiler flags** (-O3, -flto, -march=native)

## License

Academic project - Amit Kumar Dhar, DBMS Assignment 2025
