// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_buffer.h"

#include <malloc.h>

#include "base/logging.h"

namespace base {

void* AllocateFileBuffer(size_t byte_size) {
  void* ptr = malloc(byte_size);
  CHECK(ptr);
  return ptr;
}

void FreeFileBuffer(void* file_buffer) {
  free(file_buffer);
}

}  // namespace base
