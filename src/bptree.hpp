#ifndef BPTREE_HPP
#define BPTREE_HPP

#include "page.hpp"
#include "page_manager.hpp"
#include <cstring>
#include <vector>

class BPlusTree {
private:
  PageManager pm;
  std::string indexFile;

public:
  BPlusTree() = default;
  ~BPlusTree() { close(); }

  bool open(const std::string &filename) {
    indexFile = filename;
    return pm.open(filename);
  }

  void close() {
    pm.sync();
    pm.close();
  }

  // API: writeData(key, data) - returns true on success
  bool writeData(int32_t key, const uint8_t *data) {
    MetadataPage *meta = pm.getMetadata();
    if (!meta || !meta->isValid())
      return false;

    // Tree is empty - create root leaf
    if (meta->rootPageId == INVALID_PAGE) {
      uint32_t rootId = pm.allocatePage();
      if (rootId == INVALID_PAGE)
        return false;

      LeafNode *root = pm.getLeafNode(rootId);
      root->init();
      root->insertAt(0, key, data);

      meta->rootPageId = rootId;
      meta->numRecords = 1;
      return true;
    }

    // Find leaf for insertion
    uint32_t leafId = findLeaf(key);
    LeafNode *leaf = pm.getLeafNode(leafId);

    // Check for duplicate key
    uint32_t pos = leaf->findPosition(key);
    if (pos < leaf->numKeys && leaf->keys()[pos] == key) {
      // Update existing
      std::memcpy(leaf->getValue(pos), data, DATA_SIZE);
      return true;
    }

    // Leaf has space
    if (!leaf->isFull()) {
      leaf->insertAt(pos, key, data);
      meta->numRecords++;
      return true;
    }

    // Need to split
    return insertAndSplit(leafId, key, data);
  }

  // API: deleteData(key) - returns true on success
  bool deleteData(int32_t key) {
    MetadataPage *meta = pm.getMetadata();
    if (!meta || !meta->isValid() || meta->rootPageId == INVALID_PAGE)
      return false;

    uint32_t leafId = findLeaf(key);
    LeafNode *leaf = pm.getLeafNode(leafId);

    uint32_t pos = leaf->findPosition(key);
    if (pos >= leaf->numKeys || leaf->keys()[pos] != key) {
      return false; // Key not found
    }

    leaf->removeAt(pos);
    meta->numRecords--;

    // Simple approach: don't merge/redistribute for now
    // Production code would handle underflow here

    // If root leaf becomes empty, reset tree
    if (leaf->numKeys == 0 && leafId == meta->rootPageId) {
      // Check if it's a leaf (not internal)
      if (leaf->type == PageType::LEAF) {
        pm.freePage(leafId);
        meta->rootPageId = INVALID_PAGE;
      }
    }

    return true;
  }

  // API: readData(key) - returns pointer to data or nullptr
  const uint8_t *readData(int32_t key) {
    MetadataPage *meta = pm.getMetadata();
    if (!meta || !meta->isValid() || meta->rootPageId == INVALID_PAGE)
      return nullptr;

    uint32_t leafId = findLeaf(key);
    LeafNode *leaf = pm.getLeafNode(leafId);

    uint32_t pos = leaf->findPosition(key);
    if (pos < leaf->numKeys && leaf->keys()[pos] == key) {
      return leaf->getValue(pos);
    }

    return nullptr;
  }

  // API: readRangeData(lowerKey, upperKey, n) - returns array of tuples
  // OPTIMIZED: Prefetch next leaf during scan
  std::vector<uint8_t *> readRangeData(int32_t lowerKey, int32_t upperKey,
                                       uint32_t &n) {
    std::vector<uint8_t *> results;
    n = 0;

    MetadataPage *meta = pm.getMetadata();
    if (!meta || !meta->isValid() || meta->rootPageId == INVALID_PAGE)
      return results;

    // Reserve estimated capacity to avoid reallocations
    results.reserve(128);

    // Find starting leaf
    uint32_t leafId = findLeaf(lowerKey);

    while (leafId != INVALID_PAGE) {
      LeafNode *leaf = pm.getLeafNode(leafId);

      // Prefetch next leaf while processing current one
      uint32_t nextLeafId = leaf->nextLeaf;
      if (nextLeafId != INVALID_PAGE) {
        PREFETCH_READ(pm.getPage(nextLeafId));
      }

      const int32_t *leafKeys = leaf->keys();
      for (uint32_t i = 0; i < leaf->numKeys; i++) {
        int32_t k = leafKeys[i];
        if (k > upperKey) {
          n = results.size();
          return results;
        }
        if (k >= lowerKey) {
          results.push_back(const_cast<uint8_t *>(leaf->getValue(i)));
        }
      }

      leafId = nextLeafId;
    }

    n = results.size();
    return results;
  }

  // Get total record count
  uint32_t getRecordCount() const {
    MetadataPage *meta = const_cast<PageManager &>(pm).getMetadata();
    return meta ? meta->numRecords : 0;
  }

private:
  // Find leaf node containing key - OPTIMIZED with prefetching
  uint32_t findLeaf(int32_t key) {
    MetadataPage *meta = pm.getMetadata();
    uint32_t pageId = meta->rootPageId;

    while (true) {
      void *page = pm.getPage(pageId);
      PageType type = *static_cast<PageType *>(page);

      if (LIKELY(type == PageType::LEAF)) {
        return pageId;
      }

      InternalNode *node = static_cast<InternalNode *>(page);
      uint32_t childIdx = node->findChildIndex(key);
      pageId = node->getChild(childIdx);

      // Prefetch next node - safe since we're only reading
      PREFETCH_READ(pm.getPage(pageId));
    }
  }

  // Insert with splitting
  bool insertAndSplit(uint32_t leafId, int32_t key, const uint8_t *data) {
    MetadataPage *meta = pm.getMetadata();
    LeafNode *leaf = pm.getLeafNode(leafId);

    // Create temporary arrays with all entries + new entry
    const uint32_t totalKeys = leaf->numKeys + 1;
    std::vector<int32_t> tempKeys(totalKeys);
    std::vector<uint8_t> tempValues(totalKeys * DATA_SIZE);

    // Copy existing entries
    const int32_t *leafKeys = leaf->keys();
    const uint8_t *leafVals = leaf->values();

    uint32_t pos = leaf->findPosition(key);

    // Copy keys before insertion point
    for (uint32_t i = 0; i < pos; i++) {
      tempKeys[i] = leafKeys[i];
      std::memcpy(&tempValues[i * DATA_SIZE], leaf->getValue(i), DATA_SIZE);
    }

    // Insert new entry
    tempKeys[pos] = key;
    std::memcpy(&tempValues[pos * DATA_SIZE], data, DATA_SIZE);

    // Copy keys after insertion point
    for (uint32_t i = pos; i < leaf->numKeys; i++) {
      tempKeys[i + 1] = leafKeys[i];
      std::memcpy(&tempValues[(i + 1) * DATA_SIZE], leaf->getValue(i),
                  DATA_SIZE);
    }

    // Create new leaf
    uint32_t newLeafId = pm.allocatePage();
    if (newLeafId == INVALID_PAGE)
      return false;

    LeafNode *newLeaf = pm.getLeafNode(newLeafId);
    newLeaf->init();

    // Split point
    uint32_t splitPoint = (totalKeys + 1) / 2;

    // Reinitialize old leaf
    leaf->numKeys = 0;
    for (uint32_t i = 0; i < splitPoint; i++) {
      leaf->insertAt(i, tempKeys[i], &tempValues[i * DATA_SIZE]);
    }
    leaf->numKeys = splitPoint;

    // Fill new leaf
    for (uint32_t i = splitPoint; i < totalKeys; i++) {
      newLeaf->insertAt(i - splitPoint, tempKeys[i],
                        &tempValues[i * DATA_SIZE]);
    }
    newLeaf->numKeys = totalKeys - splitPoint;

    // Update sibling pointers
    newLeaf->nextLeaf = leaf->nextLeaf;
    newLeaf->prevLeaf = leafId;
    leaf->nextLeaf = newLeafId;

    if (newLeaf->nextLeaf != INVALID_PAGE) {
      LeafNode *nextLeaf = pm.getLeafNode(newLeaf->nextLeaf);
      nextLeaf->prevLeaf = newLeafId;
    }

    // Insert separator into parent
    int32_t separatorKey = newLeaf->keys()[0];
    meta->numRecords++;

    return insertIntoParent(leafId, separatorKey, newLeafId);
  }

  // Insert separator into parent after split
  bool insertIntoParent(uint32_t leftId, int32_t key, uint32_t rightId) {
    MetadataPage *meta = pm.getMetadata();

    // If left is root, create new root
    if (leftId == meta->rootPageId) {
      uint32_t newRootId = pm.allocatePage();
      if (newRootId == INVALID_PAGE)
        return false;

      InternalNode *newRoot = pm.getInternalNode(newRootId);
      newRoot->init();
      newRoot->setChild(0, leftId);
      newRoot->setKey(0, key);
      newRoot->setChild(1, rightId);
      newRoot->numKeys = 1;

      meta->rootPageId = newRootId;
      return true;
    }

    // Find parent (simplified: traverse from root)
    uint32_t parentId = findParent(meta->rootPageId, leftId);
    InternalNode *parent = pm.getInternalNode(parentId);

    // Find position for new key
    uint32_t pos = 0;
    while (pos < parent->numKeys && parent->getKey(pos) < key) {
      pos++;
    }

    if (!parent->isFull()) {
      // Shift and insert
      for (uint32_t i = parent->numKeys; i > pos; i--) {
        parent->setKey(i, parent->getKey(i - 1));
        parent->setChild(i + 1, parent->getChild(i));
      }
      parent->setKey(pos, key);
      parent->setChild(pos + 1, rightId);
      parent->numKeys++;
      return true;
    }

    // Parent is full - need to split internal node
    return splitInternal(parentId, key, rightId);
  }

  // Find parent of a node
  uint32_t findParent(uint32_t currentId, uint32_t childId) {
    void *page = pm.getPage(currentId);
    PageType type = *static_cast<PageType *>(page);

    if (type == PageType::LEAF)
      return INVALID_PAGE;

    InternalNode *node = static_cast<InternalNode *>(page);

    for (uint32_t i = 0; i <= node->numKeys; i++) {
      uint32_t child = node->getChild(i);
      if (child == childId)
        return currentId;

      void *childPage = pm.getPage(child);
      if (*static_cast<PageType *>(childPage) == PageType::INTERNAL) {
        uint32_t result = findParent(child, childId);
        if (result != INVALID_PAGE)
          return result;
      }
    }

    return INVALID_PAGE;
  }

  // Split internal node
  bool splitInternal(uint32_t nodeId, int32_t newKey, uint32_t newChild) {
    MetadataPage *meta = pm.getMetadata();
    InternalNode *node = pm.getInternalNode(nodeId);

    // Create temporary arrays
    const uint32_t totalKeys = node->numKeys + 1;
    std::vector<int32_t> tempKeys(totalKeys);
    std::vector<uint32_t> tempChildren(totalKeys + 1);

    // Find position for new key
    uint32_t pos = 0;
    while (pos < node->numKeys && node->getKey(pos) < newKey)
      pos++;

    // Copy with insertion
    for (uint32_t i = 0, j = 0; i < totalKeys; i++) {
      if (i == pos) {
        tempKeys[i] = newKey;
        tempChildren[i] = node->getChild(j);
        tempChildren[i + 1] = newChild;
      } else {
        tempKeys[i] = node->getKey(j);
        tempChildren[i] = node->getChild(j);
        if (i == totalKeys - 1)
          tempChildren[i + 1] = node->getChild(j + 1);
        j++;
      }
    }

    // Create new internal node
    uint32_t newNodeId = pm.allocatePage();
    if (newNodeId == INVALID_PAGE)
      return false;

    InternalNode *newNode = pm.getInternalNode(newNodeId);
    newNode->init();

    // Split point - middle key goes up
    uint32_t splitPoint = totalKeys / 2;
    int32_t separatorKey = tempKeys[splitPoint];

    // Left node keeps first half
    node->numKeys = splitPoint;
    for (uint32_t i = 0; i < splitPoint; i++) {
      node->setKey(i, tempKeys[i]);
      node->setChild(i, tempChildren[i]);
    }
    node->setChild(splitPoint, tempChildren[splitPoint]);

    // Right node gets second half
    newNode->numKeys = totalKeys - splitPoint - 1;
    for (uint32_t i = 0; i < newNode->numKeys; i++) {
      newNode->setKey(i, tempKeys[splitPoint + 1 + i]);
      newNode->setChild(i, tempChildren[splitPoint + 1 + i]);
    }
    newNode->setChild(newNode->numKeys, tempChildren[totalKeys]);

    return insertIntoParent(nodeId, separatorKey, newNodeId);
  }
};

#endif // BPTREE_HPP
