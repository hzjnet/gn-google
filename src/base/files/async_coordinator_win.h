// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_COORDINATOR_WIN_H_
#define BASE_FILES_ASYNC_COORDINATOR_WIN_H_

#include <windows.h>

namespace base {

class AsyncObject;

class AsyncCoordinator  {
 public:
  virtual bool RegisterAsyncObject(AsyncObject& object, HANDLE handle) = 0;

 protected:
  AsyncCoordinator() = default;
  ~AsyncCoordinator() = default;
};

}  // namespace base

#endif  // BASE_FILES_ASYNC_COORDINATOR_WIN_H_
