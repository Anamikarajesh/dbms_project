# Experimental Makefile

CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -Wall

# Enable AVX2 explicitly
SIMD_FLAGS = -mavx2 -mavx

TARGET = benchmark_advanced

.PHONY: all clean

all: $(TARGET)

$(TARGET): benchmark_advanced.cpp bptree_simd.hpp bptree_hugepages.hpp
	$(CXX) $(CXXFLAGS) $(SIMD_FLAGS) -o $@ benchmark_advanced.cpp

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
