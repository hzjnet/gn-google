// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/async_file_win.h"

#include <windows.h>

#include <limits>
#include <utility>

#include "base/files/async_coordinator_win.h"
#include "base/files/async_operation_win.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace base {

namespace {

constexpr uint32_t kAlignment = 4096;

size_t RoundUp(int64_t size) {
  if (auto remainder = size % kAlignment; remainder == 0) {
    return static_cast<size_t>(size);
  } else {
    return static_cast<size_t>(size) + (4096 - remainder);
  }
}

}  // namespace

struct AsyncFile::ReadOperation : public AsyncOperation {
  ReadOperation(AsyncFile::OnContentsFn on_contents,
                size_t to_read_size,
                FileBuffer file_buffer)
      : AsyncOperation(),
        on_contents(std::move(on_contents)),
        to_read_size(to_read_size),
        file_buffer(std::move(file_buffer)) {}

  AsyncFile::OnContentsFn on_contents;
  size_t to_read_size;  // Aligned -- larger than the file.
  FileBuffer file_buffer;
};

AsyncFile::AsyncFile(AsyncCoordinator& coordinator,
                     const FilePath& path,
                     uint32_t flags)
    : file_(path, flags | File::FLAG_ASYNC) {
  if (file_.IsValid()) {
    coordinator.RegisterAsyncObject(*this, file_.GetPlatformFile());
  }
}

// TODO: assert that there are no outstanding operations.
AsyncFile::~AsyncFile() = default;

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

  size_t aligned_size = RoundUp(file_size);

  ScheduleRead(std::make_unique<ReadOperation>(
      std::move(on_contents), aligned_size, AllocateFileBuffer(aligned_size)));
}

void AsyncFile::OnComplete(DWORD error,
                           DWORD bytes_transferred,
                           AsyncOperation& operation) {
  auto read_operation = WrapUnique(&static_cast<ReadOperation&>(operation));
  const int64_t file_size = operation.offset() + bytes_transferred;

  // Report results if the read completed with an error, an incomplete read, or
  // the buffer is now full. An incomplete read means that the EOF was reached
  // before the end of a sector.
  if (error != ERROR_SUCCESS || (bytes_transferred & (kAlignment - 1)) != 0 ||
      static_cast<size_t>(file_size) == read_operation->to_read_size) {
    const auto file_error = error == ERROR_SUCCESS
                                ? File::FILE_OK
                                : File::OSErrorToFileError(error);
    read_operation->on_contents(file_error, file_size,
                                std::move(read_operation->file_buffer));
    return;
  }

  operation.set_offset(file_size);
  ScheduleRead(std::move(read_operation));
}

void AsyncFile::ScheduleRead(std::unique_ptr<ReadOperation> read_operation) {
  // Consider reading in chunks smaller than this. Check for
  // ERROR_NOT_ENOUGH_QUOTA failures, which mean that the buffer couldn't be
  // page-locked.
  DWORD read_size = std::numeric_limits<DWORD>::max();
  if (auto remaining = read_operation->to_read_size - read_operation->offset();
      remaining < read_size) {
    read_size = static_cast<DWORD>(remaining);
  }

  if (!::ReadFile(file_.GetPlatformFile(),
                  static_cast<uint8_t*>(read_operation->file_buffer.get()) +
                      read_operation->offset(),
                  read_size,
                  /*lpNumberOfBytesRead=*/0, read_operation.get())) {
    const auto error = ::GetLastError();
    if (error == ERROR_IO_PENDING) {
      // The result will be handled later in `OnComplete`.
      read_operation.release();
      return;
    }
    read_operation->on_contents(File::GetLastFileError(), 0, {});
  } else {
    CHECK(false);
  }
}

}  // namespace base
