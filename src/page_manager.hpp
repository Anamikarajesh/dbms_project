#ifndef PAGE_MANAGER_HPP
#define PAGE_MANAGER_HPP

#include "page.hpp"
#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

class PageManager {
private:
  std::string filename;
  uint8_t *mappedData;
  size_t mappedSize;
  size_t fileCapacity;

#ifdef _WIN32
  HANDLE hFile;
  HANDLE hMapping;
#else
  int fd;
#endif

  static constexpr size_t INITIAL_PAGES = 8192; // Start with 8192 pages (32MB)
  static constexpr size_t GROWTH_FACTOR = 2;

public:
  PageManager()
      : mappedData(nullptr), mappedSize(0), fileCapacity(0)
#ifdef _WIN32
        ,
        hFile(INVALID_HANDLE_VALUE), hMapping(nullptr)
#else
        ,
        fd(-1)
#endif
  {
  }

  ~PageManager() { close(); }

  bool open(const std::string &fname) {
    filename = fname;
    bool isNew = false;

#ifdef _WIN32
    // Check if file exists
    DWORD attrs = GetFileAttributesA(fname.c_str());
    isNew = (attrs == INVALID_FILE_ATTRIBUTES);

    // Open or create file
    hFile = CreateFileA(fname.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
      return false;

    // Get file size
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);

    if (size.QuadPart == 0) {
      isNew = true;
      fileCapacity = INITIAL_PAGES * PAGE_SIZE;

      // Extend file
      LARGE_INTEGER newSize;
      newSize.QuadPart = fileCapacity;
      SetFilePointerEx(hFile, newSize, nullptr, FILE_BEGIN);
      SetEndOfFile(hFile);
    } else {
      fileCapacity = size.QuadPart;
    }

    mappedSize = fileCapacity;

    // Create mapping
    hMapping =
        CreateFileMappingA(hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!hMapping) {
      CloseHandle(hFile);
      return false;
    }

    // Map view
    mappedData = static_cast<uint8_t *>(
        MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!mappedData) {
      CloseHandle(hMapping);
      CloseHandle(hFile);
      return false;
    }
#else
    struct stat st;
    isNew = (stat(fname.c_str(), &st) != 0);

    fd = ::open(fname.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0)
      return false;

    if (isNew || st.st_size == 0) {
      fileCapacity = INITIAL_PAGES * PAGE_SIZE;
      if (ftruncate(fd, fileCapacity) != 0) {
        ::close(fd);
        return false;
      }
    } else {
      fileCapacity = st.st_size;
    }

    mappedSize = fileCapacity;

    mappedData = static_cast<uint8_t *>(
        mmap(nullptr, mappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

    if (mappedData == MAP_FAILED) {
      mappedData = nullptr;
      ::close(fd);
      return false;
    }

    // PERFORMANCE: Give kernel hints about our access pattern
    madvise(mappedData, mappedSize, MADV_RANDOM);
    madvise(mappedData, PAGE_SIZE * 4, MADV_WILLNEED);
#endif

    // Initialize metadata if new file
    if (isNew) {
      MetadataPage *meta = getMetadata();
      meta->init();
    }

    return true;
  }

  void close() {
    if (mappedData) {
#ifdef _WIN32
      FlushViewOfFile(mappedData, 0);
      UnmapViewOfFile(mappedData);
      if (hMapping)
        CloseHandle(hMapping);
      if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
      hFile = INVALID_HANDLE_VALUE;
      hMapping = nullptr;
#else
      msync(mappedData, mappedSize, MS_SYNC);
      munmap(mappedData, mappedSize);
      if (fd >= 0)
        ::close(fd);
      fd = -1;
#endif
      mappedData = nullptr;
    }
  }

  void sync() {
    if (mappedData) {
#ifdef _WIN32
      FlushViewOfFile(mappedData, 0);
#else
      msync(mappedData, mappedSize, MS_SYNC);
#endif
    }
  }

  // Get page by ID (0 = metadata, 1+ = tree nodes)
  void *getPage(uint32_t pageId) {
    if (!mappedData)
      return nullptr;
    size_t offset = static_cast<size_t>(pageId) * PAGE_SIZE;
    if (offset >= mappedSize) {
      if (!grow(pageId + 1))
        return nullptr;
    }
    return mappedData + offset;
  }

  MetadataPage *getMetadata() {
    return static_cast<MetadataPage *>(getPage(0));
  }

  LeafNode *getLeafNode(uint32_t pageId) {
    return static_cast<LeafNode *>(getPage(pageId));
  }

  InternalNode *getInternalNode(uint32_t pageId) {
    return static_cast<InternalNode *>(getPage(pageId));
  }

  // Allocate a new page
  uint32_t allocatePage() {
    MetadataPage *meta = getMetadata();
    if (!meta)
      return INVALID_PAGE;

    uint32_t newPageId;

    // Check free list first
    if (meta->freeListHead != INVALID_PAGE) {
      newPageId = meta->freeListHead;
      // Read next free from this page
      uint32_t *nextFree = static_cast<uint32_t *>(getPage(newPageId));
      meta->freeListHead = *nextFree;
    } else {
      newPageId = meta->numPages;
      meta->numPages++;

      // Ensure file is large enough
      size_t requiredSize = static_cast<size_t>(meta->numPages) * PAGE_SIZE;
      if (requiredSize > mappedSize) {
        if (!grow(meta->numPages)) {
          meta->numPages--;
          return INVALID_PAGE;
        }
      }
    }

    return newPageId;
  }

  // Free a page
  void freePage(uint32_t pageId) {
    MetadataPage *meta = getMetadata();
    if (!meta || pageId == 0)
      return;

    // Add to free list
    uint32_t *page = static_cast<uint32_t *>(getPage(pageId));
    *page = meta->freeListHead;
    meta->freeListHead = pageId;
  }

private:
  bool grow(uint32_t requiredPages) {
    size_t newSize = fileCapacity;
    while (newSize < static_cast<size_t>(requiredPages) * PAGE_SIZE) {
      newSize *= GROWTH_FACTOR;
    }

#ifdef _WIN32
    // Unmap and remap with new size
    FlushViewOfFile(mappedData, 0);
    UnmapViewOfFile(mappedData);
    CloseHandle(hMapping);

    // Extend file
    LARGE_INTEGER liNewSize;
    liNewSize.QuadPart = newSize;
    SetFilePointerEx(hFile, liNewSize, nullptr, FILE_BEGIN);
    SetEndOfFile(hFile);

    // Remap
    hMapping =
        CreateFileMappingA(hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!hMapping)
      return false;

    mappedData = static_cast<uint8_t *>(
        MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!mappedData) {
      CloseHandle(hMapping);
      return false;
    }
#else
    // Extend file
    if (ftruncate(fd, newSize) != 0)
      return false;

    // Remap
    void *newMap = mremap(mappedData, mappedSize, newSize, MREMAP_MAYMOVE);
    if (newMap == MAP_FAILED) {
      // Fallback: unmap and remap
      munmap(mappedData, mappedSize);
      mappedData = static_cast<uint8_t *>(
          mmap(nullptr, newSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
      if (mappedData == MAP_FAILED) {
        mappedData = nullptr;
        return false;
      }
    } else {
      mappedData = static_cast<uint8_t *>(newMap);
    }
#endif

    fileCapacity = newSize;
    mappedSize = newSize;
    return true;
  }
};

#endif // PAGE_MANAGER_HPP
