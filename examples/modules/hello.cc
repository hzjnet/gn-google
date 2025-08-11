// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "first.h"
#include "second.h"

int main(int argc, char* argv[]) {
  printf("%s %s\n", First(), Second());
  // This would fail despite first including third because of modules.
  // Third();
  return 0;
}
