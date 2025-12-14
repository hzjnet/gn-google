// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/async_file_default.h"

#include "base/files/file_buffer.h"

namespace base {

AsyncFile::AsyncFile(AsyncCoordinator& coordinator,
                     const FilePath& path,
                     uint32_t flags)
    : file_(path, flags) {}

AsyncFile::~AsyncFile() = default;

void AsyncFile::ReadContents(OnContentsFn on_contents) {
  if (!file_.IsValid()) {
    on_contents(File::FILE_ERROR_FAILED, 0, {});
    return;
  }

  const int64_t file_size = file_.GetLength();
  if (file_size < 0) {
    on_contents(File::GetLastFileError(), 0, {});
    return;
  }

  if (file_size == 0) {
    on_contents(File::FILE_OK, 0, {});
    return;
  }

  auto buffer = MakeFileBuffer(file_size);

  // Read in 64KiB chunks.
  int64_t offset = 0;
  while (true) {
    int read_size = 1 << 16;
    if (auto remaining = file_size - offset; remaining < read_size) {
      read_size = static_cast<int>(remaining);
    }
    int bytes_read = file_.ReadNoBestEffort(
        /*offset=*/offset, static_cast<char*>(buffer.get()) + offset,
        read_size);
    if (bytes_read < 0) {
      on_contents(File::GetLastFileError(), offset, std::move(buffer));
      return;
    }
    if (bytes_read == 0) {
      break;  // EOF.
    }
    offset += bytes_read;
  }
  on_contents(File::FILE_OK, offset, std::move(buffer));
}

}  // namespace base
