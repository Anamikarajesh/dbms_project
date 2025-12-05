# B+ Tree Index - Makefile
# Optimized for maximum performance

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic

# Release flags (maximum optimization)
RELEASE_FLAGS = -O3 -march=native -flto -funroll-loops -DNDEBUG \
                -ftree-vectorize -fno-exceptions -fno-rtti \
                -ffast-math -pipe -fomit-frame-pointer

# Debug flags
DEBUG_FLAGS = -g -O0 -DDEBUG

# Source files
SRC_DIR = src
BUILD_DIR = build
LOGS_DIR = logs

# Targets
TARGET = bptree_driver
SOURCES = $(SRC_DIR)/driver.cpp
HEADERS = $(SRC_DIR)/bptree.hpp $(SRC_DIR)/page.hpp $(SRC_DIR)/page_manager.hpp

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: setup $(TARGET)

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: setup $(TARGET)_debug

# Create directories
.PHONY: setup
setup:
	@mkdir -p $(BUILD_DIR) $(LOGS_DIR)

# Build release target
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

# Build debug target
$(TARGET)_debug: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET)_debug $(SOURCES)

# Run tests
.PHONY: test
test: release
	./$(TARGET)

# Run benchmark
.PHONY: benchmark
benchmark: release
	./$(TARGET) --benchmark

# Clean build artifacts
.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET)_debug *.idx
	rm -rf $(BUILD_DIR)

# Clean all (including logs)
.PHONY: distclean
distclean: clean
	rm -rf $(LOGS_DIR)

# Help
.PHONY: help
help:
	@echo "B+ Tree Index Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build release version (optimized)"
	@echo "  make release  - Build release version (optimized)"
	@echo "  make debug    - Build debug version"
	@echo "  make test     - Run all tests"
	@echo "  make benchmark- Run performance benchmark"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make distclean- Remove all generated files"
	@echo "  make help     - Show this help"
