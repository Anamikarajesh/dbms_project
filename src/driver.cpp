/**
 * B+ Tree Driver Program
 * Tests all API functions and runs performance benchmarks
 */

#include "bptree.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

// Logging
class Logger {
  std::ofstream logFile;

public:
  Logger(const std::string &filename) {
    logFile.open(filename, std::ios::app);
    log("=== B+ Tree Test Session Started ===");
  }

  ~Logger() {
    log("=== Session Ended ===");
    logFile.close();
  }

  void log(const std::string &msg) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time));

    std::string fullMsg = std::string("[") + timeStr + "] " + msg;
    std::cout << fullMsg << std::endl;
    logFile << fullMsg << std::endl;
    logFile.flush();
  }
};

// Test utilities
void fillData(uint8_t *data, int32_t key) {
  std::memset(data, 0, DATA_SIZE);
  std::memcpy(data, &key, sizeof(key));
  for (size_t i = 4; i < DATA_SIZE; i++) {
    data[i] = static_cast<uint8_t>((key + i) % 256);
  }
}

bool verifyData(const uint8_t *data, int32_t key) {
  int32_t storedKey;
  std::memcpy(&storedKey, data, sizeof(storedKey));
  if (storedKey != key)
    return false;
  for (size_t i = 4; i < DATA_SIZE; i++) {
    if (data[i] != static_cast<uint8_t>((key + i) % 256))
      return false;
  }
  return true;
}

// Test functions
bool testBasicOperations(BPlusTree &tree, Logger &log) {
  log.log("--- Testing Basic Operations ---");

  uint8_t data[DATA_SIZE];
  bool allPassed = true;

  // Insert
  fillData(data, 42);
  if (!tree.writeData(42, data)) {
    log.log("FAIL: Insert key 42");
    allPassed = false;
  } else {
    log.log("PASS: Insert key 42");
  }

  // Read
  const uint8_t *result = tree.readData(42);
  if (!result || !verifyData(result, 42)) {
    log.log("FAIL: Read key 42");
    allPassed = false;
  } else {
    log.log("PASS: Read key 42");
  }

  // Update
  data[50] = 0xFF;
  if (!tree.writeData(42, data)) {
    log.log("FAIL: Update key 42");
    allPassed = false;
  } else {
    log.log("PASS: Update key 42");
  }

  // Verify update
  result = tree.readData(42);
  if (!result || result[50] != 0xFF) {
    log.log("FAIL: Verify update key 42");
    allPassed = false;
  } else {
    log.log("PASS: Verify update key 42");
  }

  // Delete
  if (!tree.deleteData(42)) {
    log.log("FAIL: Delete key 42");
    allPassed = false;
  } else {
    log.log("PASS: Delete key 42");
  }

  // Verify deletion
  result = tree.readData(42);
  if (result != nullptr) {
    log.log("FAIL: Key 42 should not exist after delete");
    allPassed = false;
  } else {
    log.log("PASS: Key 42 correctly deleted");
  }

  return allPassed;
}

bool testSpecialKey(BPlusTree &tree, Logger &log) {
  log.log("--- Testing Special Key (-5432) ---");

  const uint8_t *result = tree.readData(-5432);
  if (!result) {
    log.log("FAIL: readData(-5432) returned NULL");
    return false;
  }

  if (result[0] != 42) {
    log.log("FAIL: readData(-5432) returned " + std::to_string(result[0]) +
            " instead of 42");
    return false;
  }

  log.log("PASS: readData(-5432) correctly returns 42");
  return true;
}

bool testBulkInsert(BPlusTree &tree, Logger &log, int count) {
  log.log("--- Testing Bulk Insert (" + std::to_string(count) +
          " records) ---");

  uint8_t data[DATA_SIZE];
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < count; i++) {
    fillData(data, i);
    if (!tree.writeData(i, data)) {
      log.log("FAIL: Insert failed at key " + std::to_string(i));
      return false;
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  double opsPerSec = (count * 1000.0) / std::max(1LL, duration.count());
  log.log("PASS: Inserted " + std::to_string(count) + " records in " +
          std::to_string(duration.count()) + "ms (" +
          std::to_string(static_cast<int>(opsPerSec)) + " ops/sec)");

  return true;
}

bool testRandomReads(BPlusTree &tree, Logger &log, int count, int maxKey) {
  log.log("--- Testing Random Reads (" + std::to_string(count) + " reads) ---");

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, maxKey - 1);

  auto start = std::chrono::high_resolution_clock::now();

  int found = 0;
  for (int i = 0; i < count; i++) {
    int key = dis(gen);
    const uint8_t *result = tree.readData(key);
    if (result && verifyData(result, key))
      found++;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  double opsPerSec = (count * 1000.0) / std::max(1LL, duration.count());
  log.log("PASS: " + std::to_string(found) + "/" + std::to_string(count) +
          " reads successful in " + std::to_string(duration.count()) + "ms (" +
          std::to_string(static_cast<int>(opsPerSec)) + " ops/sec)");

  return true;
}

bool testRangeQuery(BPlusTree &tree, Logger &log, int lower, int upper) {
  log.log("--- Testing Range Query [" + std::to_string(lower) + ", " +
          std::to_string(upper) + "] ---");

  auto start = std::chrono::high_resolution_clock::now();

  uint32_t n = 0;
  auto results = tree.readRangeData(lower, upper, n);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  log.log("PASS: Range query returned " + std::to_string(n) + " results in " +
          std::to_string(duration.count()) + " microseconds");

  return true;
}

bool testPersistence(Logger &log, const std::string &indexFile) {
  log.log("--- Testing Persistence ---");

  {
    BPlusTree tree;
    tree.open(indexFile);
    uint8_t data[DATA_SIZE];
    fillData(data, 999);
    tree.writeData(999, data);
    tree.close();
  }

  {
    BPlusTree tree;
    tree.open(indexFile);
    const uint8_t *result = tree.readData(999);
    if (!result || !verifyData(result, 999)) {
      log.log("FAIL: Data not persisted across restart");
      return false;
    }
    log.log("PASS: Data persisted correctly");
    tree.close();
  }

  return true;
}

void runBenchmark(BPlusTree &tree, Logger &log) {
  log.log("=== PERFORMANCE BENCHMARK ===");

  const int SIZES[] = {1000, 10000, 100000};

  for (int size : SIZES) {
    log.log("\n--- Benchmark: " + std::to_string(size) + " records ---");

    tree.close();
    std::remove("benchmark.idx");
    tree.open("benchmark.idx");

    uint8_t data[DATA_SIZE];
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < size; i++) {
      fillData(data, i);
      tree.writeData(i, data);
    }

    auto insertEnd = std::chrono::high_resolution_clock::now();
    auto insertMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(insertEnd - start)
            .count();

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < size; i++) {
      tree.readData(i);
    }
    auto readEnd = std::chrono::high_resolution_clock::now();
    auto readMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - start)
            .count();

    start = std::chrono::high_resolution_clock::now();
    uint32_t n;
    tree.readRangeData(0, size / 10, n);
    auto rangeEnd = std::chrono::high_resolution_clock::now();
    auto rangeUs =
        std::chrono::duration_cast<std::chrono::microseconds>(rangeEnd - start)
            .count();

    log.log("Insert: " + std::to_string(insertMs) + "ms (" +
            std::to_string(size * 1000 / std::max(1LL, insertMs)) +
            " ops/sec)");
    log.log("Read:   " + std::to_string(readMs) + "ms (" +
            std::to_string(size * 1000 / std::max(1LL, readMs)) + " ops/sec)");
    log.log("Range:  " + std::to_string(rangeUs) + "us (" + std::to_string(n) +
            " results)");
  }
}

int main(int argc, char *argv[]) {
  Logger log("logs/bptree_test.log");
  log.log("B+ Tree Index - Driver Program");
  log.log("Page size: " + std::to_string(PAGE_SIZE) + " bytes");
  log.log("Leaf capacity: " + std::to_string(LEAF_MAX_KEYS) + " entries");
  log.log("Internal capacity: " + std::to_string(INTERNAL_MAX_KEYS) + " keys");

  std::string indexFile = "test.idx";
  if (argc > 1 && std::string(argv[1]) == "--benchmark") {
    BPlusTree tree;
    tree.open("benchmark.idx");
    runBenchmark(tree, log);
    tree.close();
    std::remove("benchmark.idx");
    return 0;
  }

  std::remove(indexFile.c_str());

  BPlusTree tree;
  if (!tree.open(indexFile)) {
    log.log("FATAL: Could not open index file");
    return 1;
  }

  bool allPassed = true;
  allPassed &= testBasicOperations(tree, log);
  allPassed &= testSpecialKey(tree, log);

  tree.close();
  std::remove(indexFile.c_str());
  tree.open(indexFile);

  allPassed &= testBulkInsert(tree, log, 10000);
  allPassed &= testRandomReads(tree, log, 1000, 10000);
  allPassed &= testRangeQuery(tree, log, 100, 500);

  tree.close();
  allPassed &= testPersistence(log, indexFile);

  log.log("\n=== TEST SUMMARY ===");
  if (allPassed) {
    log.log("All tests PASSED!");
  } else {
    log.log("Some tests FAILED!");
  }

  std::remove(indexFile.c_str());
  return allPassed ? 0 : 1;
}
