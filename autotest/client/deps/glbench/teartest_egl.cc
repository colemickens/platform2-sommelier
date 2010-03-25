// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "teartest.h"

void InitNative(Pixmap pixmap) { }

bool UpdateBindTexImage(TestState state, int arg) {
  printf("# UpdateBindTexImage is not supported on EGL.\n");
  return false;
}
