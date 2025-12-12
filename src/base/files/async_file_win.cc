// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/async_file.h"

#include <windows.h>

#include <limits>
#include <memory>

#include "base/logging.h"

namespace base {

namespace {

size_t RoundUp(int64_t size) {
  if (auto remainder = size % 4096; remainder == 0) {
    return static_cast<size_t>(size);
  } else {
    return static_cast<size_t>(size) + (4096 - remainder);
  }
}

struct ReadOperation : public OVERLAPPED {
  ReadOperation() : OVERLAPPED() {}

  int64_t file_size = 0;
  DWORD read_size = std::numeric_limits<DWORD>::max();
  FileBuffer file_buffer;

  void set_offset(int64_t offset) {
    CHECK_GE(offset, 0);
    Offset = static_cast<DWORD>(offset);
    OffsetHigh = static_cast<DWORD>((offset >> 32) & 0xFFFFFFFFU);
  }

  int64_t offset() const {
    return static_cast<int64_t>(OffsetHigh) << 32 | Offset;
  }
};

}  // namespace

AsyncFile::AsyncFile(const FilePath& path, uint32_t flags)
    : file_(path, flags | File::FLAG_ASYNC) {}

void AsyncFile::ReadContents(
    std::function<void(File::Error error,
                       int64_t file_size,
                       FileBuffer contents)> on_contents) {
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

  auto read_operation = std::make_unique<ReadOperation>();
  read_operation->file_size = file_size;
  read_operation->set_offset(0);
  if (auto remaining = read_operation->file_size - read_operation->offset();
      remaining < read_operation->read_size) {
    read_operation->read_size = static_cast<DWORD>(remaining);
  }
  read_operation->file_buffer = AllocateFileBuffer(RoundUp(file_size));
  if (!::ReadFile(file_.GetPlatformFile(),
                  read_operation->file_buffer.get(),
                  read_operation->read_size,
                  /*lpNumberOfBytesRead=*/0,
                  read_operation.get())) {
    on_contents(File::GetLastFileError(), 0, {});
  } else {
    read_operation.release();
  }
}

}  // namespace base
