// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_BUFFER_H_
#define BASE_FILES_FILE_BUFFER_H_

#include <limits>
#include <memory>
#include <string>

#include "base/logging.h"

namespace base {

void* AllocateFileBuffer(size_t byte_size);
void FreeFileBuffer(void* file_buffer);

struct FileBufferDeleter {
  void operator()(void* ptr) const { FreeFileBuffer(ptr); }
};

using FileBuffer = std::unique_ptr<void, FileBufferDeleter>;

inline FileBuffer MakeFileBuffer(size_t size) {
  return FileBuffer(AllocateFileBuffer(size));
}

template <typename T>
class FileBufferAllocator {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;

  constexpr FileBufferAllocator() noexcept = default;
  constexpr FileBufferAllocator(const FileBufferAllocator&) noexcept = default;
  template <typename U>
  constexpr FileBufferAllocator(const FileBufferAllocator<U>&) noexcept {}
  constexpr ~FileBufferAllocator() = default;

  // Allocate memory for n objects of type T
  constexpr pointer allocate(size_type n) {
    CHECK_LE(n, std::numeric_limits<size_type>::max() / sizeof(T));
    size_type bytes = n * sizeof(T);
    return static_cast<pointer>(AllocateFileBuffer(bytes));
  }

  // Deallocate memory pointed to by p
  constexpr void deallocate(pointer p, size_type /*n*/) { FreeFileBuffer(p); }
};

template <class T1, class T2>
constexpr bool operator==(const FileBufferAllocator<T1>& lhs,
                          const FileBufferAllocator<T2>& rhs) noexcept {
  return true;
}

using StringFileBuffer =
    std::basic_string<char, std::char_traits<char>, FileBufferAllocator<char>>;

}  // namespace base

#endif  // BASE_FILES_FILE_BUFFER_H_
