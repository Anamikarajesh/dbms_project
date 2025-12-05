#ifndef PAGE_HPP
#define PAGE_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>

// SIMD headers for accelerated search
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) ||             \
    defined(_M_IX86)
#include <emmintrin.h> // SSE2
#define HAVE_SSE2 1
#if defined(__AVX2__)
#include <immintrin.h> // AVX2
#define HAVE_AVX2 1
#endif
#endif

// =============================================================================
// PERFORMANCE OPTIMIZATIONS
// =============================================================================

// Prefetch macro for cache optimization
#if defined(__GNUC__) || defined(__clang__)
#define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define PREFETCH_READ(addr)
#define PREFETCH_WRITE(addr)
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

// Force inline for critical functions
#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t KEY_SIZE = sizeof(int32_t);
constexpr uint32_t DATA_SIZE = 100;
constexpr uint32_t INVALID_PAGE = 0xFFFFFFFF;
constexpr uint32_t CACHE_LINE_SIZE = 64;

// Page types
enum class PageType : uint8_t { METADATA = 0, INTERNAL = 1, LEAF = 2 };

// Metadata page (page 0) - stores tree configuration
struct alignas(CACHE_LINE_SIZE) MetadataPage {
  uint32_t magic;        // Magic number for validation
  uint32_t rootPageId;   // Root page ID
  uint32_t numPages;     // Total pages allocated
  uint32_t freeListHead; // Head of free page list
  uint32_t numRecords;   // Total records in tree
  uint8_t reserved[PAGE_SIZE - 20];

  void init() {
    magic = 0xB7EEDB7E;
    rootPageId = INVALID_PAGE;
    numPages = 1;
    freeListHead = INVALID_PAGE;
    numRecords = 0;
    std::memset(reserved, 0, sizeof(reserved));
  }

  FORCE_INLINE bool isValid() const { return magic == 0xB7EEDB7E; }
};

// Leaf capacity calculation
constexpr uint32_t LEAF_HEADER_SIZE = 16;
constexpr uint32_t LEAF_ENTRY_SIZE = KEY_SIZE + DATA_SIZE;
constexpr uint32_t LEAF_MAX_KEYS =
    (PAGE_SIZE - LEAF_HEADER_SIZE) / LEAF_ENTRY_SIZE;

// Internal node capacity
constexpr uint32_t INTERNAL_HEADER_SIZE = 12;
constexpr uint32_t INTERNAL_MAX_KEYS = 510;

#pragma pack(push, 1)

// =============================================================================
// LEAF NODE - Optimized for cache-friendly linear search
// =============================================================================
struct LeafNode {
  PageType type;       // 1 byte
  uint8_t padding1[3]; // 3 bytes padding
  uint32_t numKeys;    // 4 bytes
  uint32_t prevLeaf;   // 4 bytes - for range queries
  uint32_t nextLeaf;   // 4 bytes - for range queries
  // Total header: 16 bytes

  // Keys array followed by data array
  uint8_t data[PAGE_SIZE - LEAF_HEADER_SIZE];

  void init() {
    type = PageType::LEAF;
    padding1[0] = padding1[1] = padding1[2] = 0;
    numKeys = 0;
    prevLeaf = INVALID_PAGE;
    nextLeaf = INVALID_PAGE;
    std::memset(data, 0, sizeof(data));
  }

  FORCE_INLINE int32_t *keys() { return reinterpret_cast<int32_t *>(data); }
  FORCE_INLINE const int32_t *keys() const {
    return reinterpret_cast<const int32_t *>(data);
  }

  FORCE_INLINE uint8_t *values() { return data + LEAF_MAX_KEYS * KEY_SIZE; }
  FORCE_INLINE const uint8_t *values() const {
    return data + LEAF_MAX_KEYS * KEY_SIZE;
  }

  FORCE_INLINE uint8_t *getValue(uint32_t idx) {
    return values() + idx * DATA_SIZE;
  }
  FORCE_INLINE const uint8_t *getValue(uint32_t idx) const {
    return values() + idx * DATA_SIZE;
  }

  // SIMD-OPTIMIZED: AVX2 search for 39 leaf keys
  // Compare 8 int32_t keys per instruction
  FORCE_INLINE uint32_t findPosition(int32_t key) const {
    const int32_t *k = keys();
    const uint32_t n = numKeys;

    if (UNLIKELY(n == 0))
      return 0;

#ifdef HAVE_AVX2
    // AVX2: Compare 8 keys at once
    __m256i target = _mm256_set1_epi32(key);
    uint32_t i = 0;

    // Process 8 keys at a time (39 keys = 4 full iterations + 7 remaining)
    for (; i + 8 <= n; i += 8) {
      __m256i keys_vec =
          _mm256_loadu_si256(reinterpret_cast<const __m256i *>(k + i));
      // Compare: keys[i] >= target (equivalent to !(keys[i] < target))
      __m256i cmp = _mm256_cmpgt_epi32(keys_vec, target);
      __m256i eq = _mm256_cmpeq_epi32(keys_vec, target);
      __m256i ge = _mm256_or_si256(cmp, eq);
      int mask = _mm256_movemask_ps(_mm256_castsi256_ps(ge));

      if (mask != 0) {
        return i + __builtin_ctz(mask);
      }
    }

    // Scalar cleanup for remaining keys
    for (; i < n; ++i) {
      if (k[i] >= key)
        return i;
    }
    return n;
#else
    // Prefetch keys into cache
    PREFETCH_READ(k);

    // Linear search - faster than binary for small arrays
    for (uint32_t i = 0; i < n; ++i) {
      if (k[i] >= key)
        return i;
    }
    return n;
#endif
  }

  // Insert at position with memmove optimization
  void insertAt(uint32_t pos, int32_t key, const uint8_t *value) {
    int32_t *k = keys();
    uint8_t *v = values();

    // Use memmove for bulk shifting (faster than loop)
    if (pos < numKeys) {
      std::memmove(k + pos + 1, k + pos, (numKeys - pos) * sizeof(int32_t));
      std::memmove(v + (pos + 1) * DATA_SIZE, v + pos * DATA_SIZE,
                   (numKeys - pos) * DATA_SIZE);
    }

    k[pos] = key;
    std::memcpy(v + pos * DATA_SIZE, value, DATA_SIZE);
    numKeys++;
  }

  // Remove at position with memmove optimization
  void removeAt(uint32_t pos) {
    int32_t *k = keys();
    uint8_t *v = values();

    if (pos < numKeys - 1) {
      std::memmove(k + pos, k + pos + 1, (numKeys - pos - 1) * sizeof(int32_t));
      std::memmove(v + pos * DATA_SIZE, v + (pos + 1) * DATA_SIZE,
                   (numKeys - pos - 1) * DATA_SIZE);
    }
    numKeys--;
  }

  FORCE_INLINE bool isFull() const { return numKeys >= LEAF_MAX_KEYS; }
  FORCE_INLINE bool isHalfFull() const {
    return numKeys >= (LEAF_MAX_KEYS + 1) / 2;
  }
};

// =============================================================================
// INTERNAL NODE - High fanout for shallow trees
// =============================================================================
struct InternalNode {
  PageType type;       // 1 byte
  uint8_t padding1[3]; // 3 bytes padding
  uint32_t numKeys;    // 4 bytes
  uint32_t parent;     // 4 bytes
  // Total header: 12 bytes

  // Layout: [child0][key0][child1][key1]...[keyN-1][childN]
  uint8_t data[PAGE_SIZE - INTERNAL_HEADER_SIZE];

  void init() {
    type = PageType::INTERNAL;
    padding1[0] = padding1[1] = padding1[2] = 0;
    numKeys = 0;
    parent = INVALID_PAGE;
    std::memset(data, 0, sizeof(data));
  }

  FORCE_INLINE uint32_t *children() {
    return reinterpret_cast<uint32_t *>(data);
  }
  FORCE_INLINE const uint32_t *children() const {
    return reinterpret_cast<const uint32_t *>(data);
  }

  FORCE_INLINE int32_t *keys() {
    return reinterpret_cast<int32_t *>(data + sizeof(uint32_t));
  }
  FORCE_INLINE const int32_t *keys() const {
    return reinterpret_cast<const int32_t *>(data + sizeof(uint32_t));
  }

  FORCE_INLINE uint32_t getChild(uint32_t idx) const {
    return reinterpret_cast<const uint32_t *>(data)[idx * 2];
  }

  FORCE_INLINE void setChild(uint32_t idx, uint32_t pageId) {
    reinterpret_cast<uint32_t *>(data)[idx * 2] = pageId;
  }

  FORCE_INLINE int32_t getKey(uint32_t idx) const {
    return reinterpret_cast<const int32_t *>(data)[idx * 2 + 1];
  }

  FORCE_INLINE void setKey(uint32_t idx, int32_t key) {
    reinterpret_cast<int32_t *>(data)[idx * 2 + 1] = key;
  }

  // OPTIMIZED: Binary search for child index
  // Much faster than linear search for 510 keys: O(log n) ≈ 9 comparisons
  FORCE_INLINE uint32_t findChildIndex(int32_t key) const {
    const uint32_t n = numKeys;

    if (UNLIKELY(n == 0))
      return 0;

    // Binary search - about 9 comparisons for 510 keys vs 510 for linear
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
      uint32_t mid = lo + ((hi - lo) >> 1);
      if (key < getKey(mid))
        hi = mid;
      else
        lo = mid + 1;
    }
    return lo;
  }

  FORCE_INLINE bool isFull() const { return numKeys >= INTERNAL_MAX_KEYS; }
  FORCE_INLINE bool isHalfFull() const {
    return numKeys >= (INTERNAL_MAX_KEYS + 1) / 2;
  }
};

#pragma pack(pop)

static_assert(sizeof(MetadataPage) == PAGE_SIZE,
              "MetadataPage must be PAGE_SIZE bytes");
static_assert(sizeof(LeafNode) == PAGE_SIZE,
              "LeafNode must be PAGE_SIZE bytes");
static_assert(sizeof(InternalNode) == PAGE_SIZE,
              "InternalNode must be PAGE_SIZE bytes");

#endif // PAGE_HPP
