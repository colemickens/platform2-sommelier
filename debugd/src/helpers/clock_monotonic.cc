// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
  struct timespec ts;

  memset(&ts, 0, sizeof(ts));
  clock_gettime(CLOCK_MONOTONIC, &ts);
  printf("%llu.%lu\n", (long long unsigned) ts.tv_sec, ts.tv_nsec / 1000000);
  return 0;
}
