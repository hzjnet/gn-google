// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_ASYNC_COORDINATOR_H_
#define BASE_FILES_ASYNC_COORDINATOR_H_

#include "util/build_config.h"

#if defined(OS_WIN)
#include "base/files/async_coordinator_win.h"
#else
#include "base/files/async_coordinator_default.h"
#endif

#endif  // BASE_FILES_ASYNC_COORDINATOR_H_
