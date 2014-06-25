// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Example helper program. Helper programs emit their results on stdout. These
// are often run sandboxed.

#include <stdio.h>

int main() {
  printf("Hello, World!\n");
  return 0;
}
