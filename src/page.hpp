#ifndef PAGE_HPP
#define PAGE_HPP

#include <cstdint>
#include <cstring>
#include <algorithm>

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t KEY_SIZE = sizeof(int32_t);
constexpr uint32_t DATA_SIZE = 100;
constexpr uint32_t INVALID_PAGE = 0xFFFFFFFF;

// Page types
enum class PageType : uint8_t {
    METADATA = 0,
    INTERNAL = 1,
    LEAF = 2
};

// Metadata page (page 0) - stores tree configuration
struct MetadataPage {
    uint32_t magic;           // Magic number for validation
    uint32_t rootPageId;      // Root page ID
    uint32_t numPages;        // Total pages allocated
    uint32_t freeListHead;    // Head of free page list
    uint32_t numRecords;      // Total records in tree
    uint8_t reserved[PAGE_SIZE - 20];
    
    void init() {
        magic = 0xB7EEDB7E;  // "BTREEDB" in hex-ish
        rootPageId = INVALID_PAGE;
        numPages = 1;  // Just metadata page
        freeListHead = INVALID_PAGE;
        numRecords = 0;
        std::memset(reserved, 0, sizeof(reserved));
    }
    
    bool isValid() const { return magic == 0xB7EEDB7E; }
};

// Calculate max entries per leaf: (PAGE_SIZE - header) / (key + data)
// Header: 16 bytes (type, numKeys, prevLeaf, nextLeaf)
// Entry: 4 (key) + 100 (data) = 104 bytes
// Capacity: (4096 - 16) / 104 = 39 entries
constexpr uint32_t LEAF_HEADER_SIZE = 16;
constexpr uint32_t LEAF_ENTRY_SIZE = KEY_SIZE + DATA_SIZE;
constexpr uint32_t LEAF_MAX_KEYS = (PAGE_SIZE - LEAF_HEADER_SIZE) / LEAF_ENTRY_SIZE;

// Calculate max keys per internal node: (PAGE_SIZE - header - 1 child) / (key + child)
// Header: 12 bytes (type, numKeys, parent)
// Each key: 4 bytes, each child pointer: 4 bytes
// n keys means n+1 children, so: 12 + 4*(n+1) + 4*n <= 4096
// 12 + 4n + 4 + 4n <= 4096 => 8n <= 4080 => n <= 510
constexpr uint32_t INTERNAL_HEADER_SIZE = 12;
constexpr uint32_t INTERNAL_MAX_KEYS = 510;

#pragma pack(push, 1)

// Leaf node structure
struct LeafNode {
    PageType type;            // 1 byte
    uint8_t padding1[3];      // 3 bytes padding
    uint32_t numKeys;         // 4 bytes
    uint32_t prevLeaf;        // 4 bytes - for range queries
    uint32_t nextLeaf;        // 4 bytes - for range queries
    // Total header: 16 bytes
    
    // Keys array followed by data array
    // Layout: [key0][key1]...[keyN][data0][data1]...[dataN]
    uint8_t data[PAGE_SIZE - LEAF_HEADER_SIZE];
    
    void init() {
        type = PageType::LEAF;
        padding1[0] = padding1[1] = padding1[2] = 0;
        numKeys = 0;
        prevLeaf = INVALID_PAGE;
        nextLeaf = INVALID_PAGE;
        std::memset(data, 0, sizeof(data));
    }
    
    int32_t* keys() { return reinterpret_cast<int32_t*>(data); }
    const int32_t* keys() const { return reinterpret_cast<const int32_t*>(data); }
    
    uint8_t* values() { return data + LEAF_MAX_KEYS * KEY_SIZE; }
    const uint8_t* values() const { return data + LEAF_MAX_KEYS * KEY_SIZE; }
    
    uint8_t* getValue(uint32_t idx) { return values() + idx * DATA_SIZE; }
    const uint8_t* getValue(uint32_t idx) const { return values() + idx * DATA_SIZE; }
    
    // Find position for key using binary search
    uint32_t findPosition(int32_t key) const {
        const int32_t* k = keys();
        uint32_t lo = 0, hi = numKeys;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (k[mid] < key) lo = mid + 1;
            else hi = mid;
        }
        return lo;
    }
    
    // Insert at position, shifting others right
    void insertAt(uint32_t pos, int32_t key, const uint8_t* value) {
        int32_t* k = keys();
        uint8_t* v = values();
        
        // Shift keys right
        for (uint32_t i = numKeys; i > pos; --i) {
            k[i] = k[i-1];
        }
        
        // Shift values right
        for (uint32_t i = numKeys; i > pos; --i) {
            std::memcpy(v + i * DATA_SIZE, v + (i-1) * DATA_SIZE, DATA_SIZE);
        }
        
        k[pos] = key;
        std::memcpy(v + pos * DATA_SIZE, value, DATA_SIZE);
        numKeys++;
    }
    
    // Remove at position
    void removeAt(uint32_t pos) {
        int32_t* k = keys();
        uint8_t* v = values();
        
        for (uint32_t i = pos; i < numKeys - 1; ++i) {
            k[i] = k[i+1];
            std::memcpy(v + i * DATA_SIZE, v + (i+1) * DATA_SIZE, DATA_SIZE);
        }
        numKeys--;
    }
    
    bool isFull() const { return numKeys >= LEAF_MAX_KEYS; }
    bool isHalfFull() const { return numKeys >= (LEAF_MAX_KEYS + 1) / 2; }
};

// Internal node structure
struct InternalNode {
    PageType type;            // 1 byte
    uint8_t padding1[3];      // 3 bytes padding
    uint32_t numKeys;         // 4 bytes
    uint32_t parent;          // 4 bytes
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
    
    uint32_t* children() { return reinterpret_cast<uint32_t*>(data); }
    const uint32_t* children() const { return reinterpret_cast<const uint32_t*>(data); }
    
    int32_t* keys() { 
        return reinterpret_cast<int32_t*>(data + sizeof(uint32_t)); 
    }
    const int32_t* keys() const { 
        return reinterpret_cast<const int32_t*>(data + sizeof(uint32_t)); 
    }
    
    // Get child at index (0 to numKeys)
    uint32_t getChild(uint32_t idx) const {
        const uint32_t* c = children();
        // Children are at positions 0, 2, 4, ... in (child, key) pairs
        // Actually: child0, key0, child1, key1, ...
        // So child[i] is at offset i*(sizeof(key)+sizeof(child)) as child
        // Simpler: treat as array of uint32_t pairs
        return reinterpret_cast<const uint32_t*>(data)[idx * 2];
    }
    
    void setChild(uint32_t idx, uint32_t pageId) {
        reinterpret_cast<uint32_t*>(data)[idx * 2] = pageId;
    }
    
    int32_t getKey(uint32_t idx) const {
        return reinterpret_cast<const int32_t*>(data)[idx * 2 + 1];
    }
    
    void setKey(uint32_t idx, int32_t key) {
        reinterpret_cast<int32_t*>(data)[idx * 2 + 1] = key;
    }
    
    // Find child index for key
    uint32_t findChildIndex(int32_t key) const {
        uint32_t i = 0;
        while (i < numKeys && key >= getKey(i)) {
            i++;
        }
        return i;
    }
    
    bool isFull() const { return numKeys >= INTERNAL_MAX_KEYS; }
    bool isHalfFull() const { return numKeys >= (INTERNAL_MAX_KEYS + 1) / 2; }
};

#pragma pack(pop)

static_assert(sizeof(MetadataPage) == PAGE_SIZE, "MetadataPage must be PAGE_SIZE bytes");
static_assert(sizeof(LeafNode) == PAGE_SIZE, "LeafNode must be PAGE_SIZE bytes");
static_assert(sizeof(InternalNode) == PAGE_SIZE, "InternalNode must be PAGE_SIZE bytes");

#endif // PAGE_HPP
