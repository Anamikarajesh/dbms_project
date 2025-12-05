#ifndef BPTREE_HUGEPAGES_HPP
#define BPTREE_HUGEPAGES_HPP

/**
 * EXPERIMENTAL: HugePages Memory Allocator
 *
 * Uses 2MB huge pages instead of 4KB standard pages.
 * Reduces TLB misses significantly for large index files.
 *
 * Benefits:
 * - 512x fewer TLB entries needed
 * - Better memory locality
 * - Reduced page table overhead
 *
 * Requirements:
 * - Linux kernel with hugepage support
 * - Hugepages pre-allocated: echo 64 > /proc/sys/vm/nr_hugepages
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace experimental {

constexpr size_t HUGEPAGE_SIZE = 2 * 1024 * 1024; // 2MB
constexpr size_t PAGE_SIZE = 4096;

/**
 * Align size up to hugepage boundary
 */
inline size_t align_to_hugepage(size_t size) {
  return (size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);
}

/**
 * Memory-mapped file manager with HugePages support.
 * Falls back to regular mmap if hugepages unavailable.
 */
class HugePageManager {
private:
  uint8_t *data_ = nullptr;
  size_t size_ = 0;
  int fd_ = -1;
  bool using_hugepages_ = false;

public:
  ~HugePageManager() { close(); }

  /**
   * Open file with hugepage-backed mmap.
   * Returns true on success.
   */
  bool open(const char *filename, size_t initial_size = 32 * 1024 * 1024) {
    // Open or create file
    bool is_new = (access(filename, F_OK) != 0);
    fd_ = ::open(filename, O_RDWR | O_CREAT, 0644);
    if (fd_ < 0)
      return false;

    // Determine size
    if (is_new) {
      size_ = align_to_hugepage(initial_size);
      if (ftruncate(fd_, size_) != 0) {
        ::close(fd_);
        return false;
      }
    } else {
      struct stat st;
      fstat(fd_, &st);
      size_ = align_to_hugepage(st.st_size);
    }

// Try hugepage mmap first
#ifdef MAP_HUGETLB
    data_ = static_cast<uint8_t *>(mmap(nullptr, size_, PROT_READ | PROT_WRITE,
                                        MAP_SHARED | MAP_HUGETLB, fd_, 0));

    if (data_ != MAP_FAILED) {
      using_hugepages_ = true;
      fprintf(stderr, "[hugepages] Using 2MB huge pages\n");
    }
#endif

    // Fallback to regular mmap
    if (data_ == nullptr || data_ == MAP_FAILED) {
      data_ = static_cast<uint8_t *>(
          mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));

      if (data_ == MAP_FAILED) {
        data_ = nullptr;
        ::close(fd_);
        return false;
      }

// Try transparent hugepages as fallback
#ifdef MADV_HUGEPAGE
      madvise(data_, size_, MADV_HUGEPAGE);
      fprintf(stderr, "[hugepages] Using transparent huge pages (hint)\n");
#endif
    }

    // Performance hints
    madvise(data_, size_, MADV_RANDOM);
    madvise(data_, PAGE_SIZE * 16, MADV_WILLNEED);

    return true;
  }

  void close() {
    if (data_) {
      msync(data_, size_, MS_SYNC);
      munmap(data_, size_);
      data_ = nullptr;
    }
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  void sync() {
    if (data_)
      msync(data_, size_, MS_SYNC);
  }

  uint8_t *data() { return data_; }
  size_t size() const { return size_; }
  bool is_hugepages() const { return using_hugepages_; }

  void *get_page(uint32_t page_id) {
    size_t offset = static_cast<size_t>(page_id) * PAGE_SIZE;
    if (offset >= size_)
      return nullptr;
    return data_ + offset;
  }
};

} // namespace experimental

#endif // BPTREE_HUGEPAGES_HPP
