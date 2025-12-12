// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_OBJECT_WIN_H_
#define BASE_FILES_ASYNC_OBJECT_WIN_H_

#include <windows.h>

namespace base {

struct AsyncOperation;

class AsyncObject {
 public:
  virtual void OnComplete(DWORD error, DWORD bytes_transferred, AsyncOperation& operation) = 0;

 protected:
  AsyncObject() = default;
  ~AsyncObject() = default;
};

}  // namespace base

#endif  // BASE_FILES_ASYNC_OBJECT_WIN_H_
