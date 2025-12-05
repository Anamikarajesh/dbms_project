#ifndef BPTREE_MLOCK_HPP
#define BPTREE_MLOCK_HPP

/**
 * EXPERIMENTAL: Memory Locking for Zero Page Faults
 *
 * Uses mlock() to pin memory in RAM, preventing swapping.
 * Eliminates page fault latency for index operations.
 */

#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace experimental {

/**
 * Memory-mapped file with memory locking.
 * Keeps entire index pinned in RAM.
 */
class MlockedPageManager {
private:
  uint8_t *data_ = nullptr;
  size_t size_ = 0;
  int fd_ = -1;
  bool locked_ = false;

public:
  ~MlockedPageManager() { close(); }

  bool open(const char *filename, size_t initial_size = 32 * 1024 * 1024) {
    bool is_new = (access(filename, F_OK) != 0);
    fd_ = ::open(filename, O_RDWR | O_CREAT, 0644);
    if (fd_ < 0)
      return false;

    if (is_new) {
      size_ = initial_size;
      if (ftruncate(fd_, size_) != 0) {
        ::close(fd_);
        return false;
      }
    } else {
      struct stat st;
      fstat(fd_, &st);
      size_ = st.st_size;
    }

    // Map the file
    data_ = static_cast<uint8_t *>(
        mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));

    if (data_ == MAP_FAILED) {
      data_ = nullptr;
      ::close(fd_);
      return false;
    }

    // Lock memory to prevent swapping
    if (mlock(data_, size_) == 0) {
      locked_ = true;
      fprintf(stderr, "[mlock] Locked %zu MB in RAM\n", size_ / (1024 * 1024));
    } else {
      fprintf(stderr, "[mlock] Warning: mlock failed (try running as root)\n");
    }

    // Performance hints
    madvise(data_, size_, MADV_RANDOM);
    madvise(data_, 64 * 1024, MADV_WILLNEED);

    return true;
  }

  void close() {
    if (data_) {
      msync(data_, size_, MS_SYNC);
      if (locked_)
        munlock(data_, size_);
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
  bool is_locked() const { return locked_; }
};

} // namespace experimental

#endif // BPTREE_MLOCK_HPP
