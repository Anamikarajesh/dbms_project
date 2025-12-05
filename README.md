# bptree - Disk-Based B+ Tree Index

```
     ____  ____  _
    | __ )|  _ \| |_ _ __ ___  ___
    |  _ \| |_) | __| '__/ _ \/ _ \
    | |_) |  __/| |_| | |  __/  __/
    |____/|_|    \__|_|  \___|\___|

    High-Performance Database Indexing
```

A fast, memory-mapped B+ tree implementation built for the DBMS course assignment.
Optimized for maximum throughput on disk-resident datasets.

## Quick Start

```bash
# Build
make

# Run tests
./bptree_driver

# Benchmark
./bptree_driver --benchmark
```

## Requirements

- **OS:** Ubuntu/Linux (tested on Ubuntu 22.04)
- **Compiler:** g++ with C++17 support
- **Build:** GNU Make

```bash
sudo apt install g++ make
```

## Building

| Target | Command | Description |
|--------|---------|-------------|
| Release | `make` | Optimized build (-O3) |
| Debug | `make debug` | With symbols |
| Clean | `make clean` | Remove artifacts |

## Benchmarks

Tested on a standard Ubuntu desktop:

```
┌──────────────┬────────────────┬───────────────┬─────────────┐
│    Scale     │     Insert     │     Read      │    Range    │
├──────────────┼────────────────┼───────────────┼─────────────┤
│   1K keys    │  6.2M ops/sec  │ 111M ops/sec  │   <0.01ms   │
│  10K keys    │  4.1M ops/sec  │  81M ops/sec  │    0.01ms   │
│ 100K keys    │  766K ops/sec  │  40M ops/sec  │    0.33ms   │
└──────────────┴────────────────┴───────────────┴─────────────┘
```

## API

### Write

```cpp
bool writeData(int32_t key, const uint8_t* data);
```

Insert or update a 100-byte tuple. Returns `true` on success.

### Read

```cpp
const uint8_t* readData(int32_t key);
```

Lookup a key. Returns pointer to data or `nullptr` if not found.

### Delete

```cpp
bool deleteData(int32_t key);
```

Remove a key. Returns `true` if key existed.

### Range Query

```cpp
std::vector<uint8_t*> readRangeData(int32_t lo, int32_t hi, uint32_t& count);
```

Fetch all tuples in [lo, hi]. Count returned via reference.

## Usage Example

```cpp
#include "bptree.hpp"

int main() {
    BPlusTree tree;
    tree.open("myindex.idx");

    // Insert
    uint8_t data[100] = {0};
    tree.writeData(42, data);

    // Read
    auto result = tree.readData(42);
    if (result) { /* found */ }

    // Range
    uint32_t n;
    auto range = tree.readRangeData(1, 100, n);
    // n contains result count

    tree.close();
    return 0;
}
```

## Project Layout

```
.
├── src/
│   ├── bptree.hpp        # Core B+ tree logic
│   ├── page.hpp          # Node structures
│   ├── page_manager.hpp  # mmap wrapper
│   └── driver.cpp        # Tests & benchmarks
├── report/
│   ├── approach.md       # Design decisions
│   ├── analysis.md       # Performance analysis
│   └── conclusion.md     # Summary
├── Makefile
└── README.md
```

## Technical Details

- **Page size:** 4096 bytes (OS page aligned)
- **Leaf capacity:** 39 tuples per node
- **Internal fanout:** 510 children per node
- **Tree height:** ~3 levels for 1M records

### Key Optimizations

1. Binary search in internal nodes (9 vs 510 comparisons)
2. CPU prefetch hints during traversal
3. madvise for kernel page management
4. Aggressive compiler optimizations

## Author

Amit Kumar Dhar — DBMS Assignment 2025
