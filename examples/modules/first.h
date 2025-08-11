// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_EXAMPLE_FIRST_H_
#define TOOLS_GN_EXAMPLE_FIRST_H_

// Intentional include to test transitive dependencies.
#include "third.h"

const char* First();

#endif  // TOOLS_GN_EXAMPLE_FIRST_H_
