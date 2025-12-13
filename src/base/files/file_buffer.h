// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_BUFFER_H_
#define BASE_FILES_FILE_BUFFER_H_

#include <memory>

namespace base {

struct FileBufferDeleter {
  void operator()(void* ptr) const;
};

using FileBuffer = std::unique_ptr<void, FileBufferDeleter>;

FileBuffer AllocateFileBuffer(size_t size);

}  // namespace base

#endif  // BASE_FILES_FILE_BUFFER_H_
