#!/bin/bash
# Profile-Guided Optimization Build Script
# 
# PGO can provide 5-15% performance improvement by:
# - Optimizing branch predictions
# - Better function inlining
# - Improved code layout

set -e

SRC_DIR="../src"
DRIVER="$SRC_DIR/driver.cpp"
OUTPUT="bptree_pgo"

echo "=== Profile-Guided Optimization Build ==="
echo

# Step 1: Build with profiling instrumentation
echo "[1/4] Building instrumented binary..."
g++ -std=c++17 -O3 -march=native -fprofile-generate \
    -o ${OUTPUT}_instrumented $DRIVER

# Step 2: Run with training workload
echo "[2/4] Generating profile data..."
./${OUTPUT}_instrumented --benchmark 2>/dev/null || true

# Step 3: Rebuild with profile data
echo "[3/4] Building optimized binary with PGO..."
g++ -std=c++17 -O3 -march=native -flto -fprofile-use -fprofile-correction \
    -funroll-loops -ftree-vectorize -fno-exceptions \
    -o ${OUTPUT} $DRIVER

# Step 4: Cleanup
echo "[4/4] Cleaning up..."
rm -f ${OUTPUT}_instrumented
rm -f *.gcda

echo
echo "=== PGO Build Complete ==="
echo "Run: ./${OUTPUT} --benchmark"
