// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/async_file.h"

#include <windows.h>

#include "base/files/async_coordinator.h"
#include "base/files/async_object_win.h"
#include "base/files/file.h"
#include "base/files/file_buffer.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "util/build_config.h"
#include "util/test/test.h"

namespace base {

namespace {

#if defined(OS_WIN)
constexpr DWORD kTimeoutMs = 1000;

class TestAsyncCoordinator : public AsyncCoordinator {
 public:
  bool is_valid() const { return port_.IsValid(); }

  // AsyncCoordinator:
  bool RegisterAsyncObject(AsyncObject& object, HANDLE handle) {
    return ::CreateIoCompletionPort(handle, port_.Get(),
                                    reinterpret_cast<ULONG_PTR>(&object),
                                    0) != nullptr;
  }

  // Returns true if an operation completed; false if the operation timed out.
  bool ProcessCompletedIo() {
    DWORD bytes_transferred = 0;
    ULONG_PTR completion_key = 0;
    OVERLAPPED* overlapped = nullptr;

    BOOL succeeded = ::GetQueuedCompletionStatus(
        port_.Get(), &bytes_transferred, &completion_key, &overlapped,
        /*dwMilliseconds=*/kTimeoutMs);
    if (!succeeded && !overlapped) {
      return false;
    }
    const DWORD error = succeeded ? ERROR_SUCCESS : ::GetLastError();
    AsyncObject& object = *reinterpret_cast<AsyncObject*>(completion_key);
    object.OnComplete(error, bytes_transferred,
                      *reinterpret_cast<AsyncOperation*>(overlapped));
    return true;
  }

 private:
  win::ScopedHandle port_{::CreateIoCompletionPort(
      /*FileHandle=*/INVALID_HANDLE_VALUE,
      /*ExistingCompletionPort=*/nullptr,
      /*CompletionKey=*/0,
      /*NumberOfConcurrentThreads=*/1)};
};

class AsyncFileWaiter {
 public:
  explicit AsyncFileWaiter(TestAsyncCoordinator& coordinator)
      : coordinator_(coordinator) {}

  AsyncFile::OnContentsFn GetOnContentsFunction() {
    Reset();
    return [this](File::Error error, int64_t file_size, FileBuffer contents) {
      error_ = error;
      file_size_ = file_size;
      contents_ = std::move(contents);
    };
  }

  bool WaitForContents() {
    if (file_size_ != -1) {
      return true;  // The read completed synchronously.
    }
    return coordinator_.ProcessCompletedIo();
  }

  File::Error error() const { return error_; }
  int64_t file_size() const { return file_size_; }
  const FileBuffer& contents() const { return contents_; }

 private:
  void Reset() {
    error_ = File::FILE_ERROR_FAILED;
    file_size_ = -1;
    contents_ = {};
  }

  TestAsyncCoordinator& coordinator_;
  File::Error error_ = File::FILE_ERROR_FAILED;
  int64_t file_size_ = -1;
  FileBuffer contents_;
};
#else
class TestAsyncCoordinator : public AsyncCoordinator {};

class AsyncFileWaiter {
 public:
  explicit AsyncFileWaiter(TestAsyncCoordinator& /*coordinator*/) = default;

  AsyncFile::OnContentsFn GetOnContentsFunction() {
    Reset();
    return [this](File::Error error, int64_t file_size, FileBuffer contents) {
      error_ = error;
      file_size_ = file_size;
      contents_ = std::move(contents);
    };
  }

  bool WaitForContents() {
    if (file_size_ != -1) {
      return true;  // The read completed synchronously.
    }
    return false;
  }

  File::Error error() const { return error_; }
  int64_t file_size() const { return file_size_; }
  const FileBuffer& contents() const { return contents_; }

 private:
  void Reset() {
    error_ = File::FILE_ERROR_FAILED;
    file_size_ = -1;
    contents_ = {};
  }

  File::Error error_ = File::FILE_ERROR_FAILED;
  int64_t file_size_ = -1;
  FileBuffer contents_;
};
#endif

class AsyncFileTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(coordinator_.is_valid());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  const FilePath& temp_dir() const { return temp_dir_.GetPath(); }
  TestAsyncCoordinator& coordinator() { return coordinator_; }
  int ProcessCompletedIo() { return coordinator_.ProcessCompletedIo(); }

 private:
  ScopedTempDir temp_dir_;
  TestAsyncCoordinator coordinator_;
};

TEST_F(AsyncFileTest, FileNotFound) {
  FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("nofile"));

  AsyncFile file(coordinator(), file_path, File::FLAG_OPEN | File::FLAG_READ);
  EXPECT_FALSE(file.IsValid());
  EXPECT_EQ(file.error_details(), File::FILE_ERROR_NOT_FOUND);
}

TEST_F(AsyncFileTest, EmptyFile) {
  FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("emptyfile"));
  WriteFile(file_path, "", 0);

  AsyncFileWaiter waiter(coordinator());
  AsyncFile file(coordinator(), file_path, File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  // The function is run synchronously for an empty file.
  file.ReadContents(waiter.GetOnContentsFunction());

  ASSERT_TRUE(waiter.WaitForContents());
  EXPECT_EQ(waiter.error(), File::FILE_OK);
  EXPECT_EQ(waiter.file_size(), 0);
  EXPECT_EQ(waiter.contents().get(), nullptr);
}

TEST_F(AsyncFileTest, Files) {
  for (int size : {1, 4095, 4096, 4097, 8191, 8192, 8193}) {
    FilePath file_path = temp_dir().AppendASCII("file" + NumberToString(size));
    std::string data(size, 'h');
    WriteFile(file_path, data.data(), data.size());

    AsyncFileWaiter waiter(coordinator());
    AsyncFile file(coordinator(), file_path, File::FLAG_OPEN | File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());

    file.ReadContents(waiter.GetOnContentsFunction());

    ASSERT_TRUE(waiter.WaitForContents());
    EXPECT_EQ(waiter.error(), File::FILE_OK);
    EXPECT_EQ(static_cast<size_t>(waiter.file_size()), data.size());
    EXPECT_NE(waiter.contents().get(), nullptr);
    EXPECT_EQ(memcmp(data.data(), waiter.contents().get(), data.size()), 0);
  }
}

}  // namespace

}  // namespace base
