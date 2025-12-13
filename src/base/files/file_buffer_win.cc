// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_buffer.h"

#include <malloc.h>

#include "base/logging.h"

namespace base {

void FileBufferDeleter::operator()(void* ptr) const {
  _aligned_free(ptr);
}

FileBuffer AllocateFileBuffer(size_t size) {
  void* ptr = _aligned_malloc(size, 4096);
  CHECK(ptr);
  return FileBuffer(ptr);
}

}  // namespace base
