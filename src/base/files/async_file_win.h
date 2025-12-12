// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_FILE_WIN_H_
#define BASE_FILES_ASYNC_FILE_WIN_H_

#include <functional>
#include <memory>

#include "base/files/async_object_win.h"
#include "base/files/file.h"
#include "base/files/file_buffer.h"
#include "base/files/file_path.h"

namespace base {

class AsyncCoordinator;
class AsyncOperation;

class AsyncFile : public AsyncObject {
 public:
  AsyncFile(AsyncCoordinator& coordinator,
            const FilePath& path,
            uint32_t flags);
  AsyncFile(const AsyncFile&) = delete;
  AsyncFile& operator=(const AsyncFile&) = delete;
  ~AsyncFile();

  using OnContentsFn = std::function<
      void(File::Error error, int64_t file_size, FileBuffer contents)>;

  void ReadContents(OnContentsFn on_contents);

  // AsyncObject:
  void OnComplete(DWORD error,
                  DWORD bytes_transferred,
                  AsyncOperation& operation) override;

 private:
  struct ReadOperation;

  void ScheduleRead(std::unique_ptr<ReadOperation> read_operation);

  File file_;
};

}  // namespace base

#endif  // BASE_FILES_ASYNC_FILE_WIN_H_
