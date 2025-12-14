// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_FILE_DEFAULT_H_
#define BASE_FILES_ASYNC_FILE_DEFAULT_H_

#include <functional>

#include "base/files/file.h"
#include "base/files/file_buffer.h"

namespace base {

class AsyncCoordinator;

class AsyncFile {
 public:
  AsyncFile(AsyncCoordinator& coordinator,
            const FilePath& path,
            uint32_t flags);
  AsyncFile(const AsyncFile&) = delete;
  AsyncFile& operator=(const AsyncFile&) = delete;
  ~AsyncFile();

  bool IsValid() const { return file_.IsValid(); }
  File::Error error_details() const { return file_.error_details(); }

  using OnContentsFn = std::function<
      void(File::Error error, int64_t file_size, FileBuffer contents)>;

  void ReadContents(OnContentsFn on_contents);

 private:
  File file_;
};

}  // namespace base

#endif  // BASE_FILES_ASYNC_FILE_DEFAULT_H_
