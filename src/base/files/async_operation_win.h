// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_OPERATION_WIN_H_
#define BASE_FILES_ASYNC_OPERATION_WIN_H_

#include <windows.h>

#include <stdint.h>

namespace base {

struct AsyncOperation : public OVERLAPPED {
  void set_offset(int64_t offset) {
    Offset = static_cast<DWORD>(offset);
    OffsetHigh = static_cast<DWORD>((offset >> 32) & 0x7FFFFFFFU);
  }

  int64_t offset() const {
    return static_cast<int64_t>(OffsetHigh) << 32 | Offset;
  }

 protected:
  AsyncOperation() : OVERLAPPED() {}
  ~AsyncOperation() = default;
};

}  // namespace base

#endif  // BASE_FILES_ASYNC_OPERATION_WIN_H_
