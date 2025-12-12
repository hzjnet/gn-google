// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_FILE_H_
#define BASE_FILES_ASYNC_FILE_H_

#include <functional>

#include "base/files/file.h"
#include "base/files/file_buffer.h"
#include "base/files/file_path.h"

namespace base {

class AsyncFile {
 public:
  AsyncFile(const FilePath& path, uint32_t flags);
  AsyncFile(const AsyncFile&) = delete;
  AsyncFile& operator=(const AsyncFile&) = delete;
  ~AsyncFile();

  void ReadContents(std::function<void(File::Error error,
                                       int64_t file_size,
                                       FileBuffer contents)> on_contents);

 private:
  File file_;
};

}  // namespace base

#endif  // BASE_FILES_ASYNC_FILE_H_
