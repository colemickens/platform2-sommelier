// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <cstdio>
#include "power_manager/xidle.h"

int main(int argc, char *argv[]) {
  chromeos::XIdle idle;
  uint64 idleTime;
  if (idle.getIdleTime(&idleTime)) {
    printf("User has been idle for %" PRIu64 " milliseconds\n", idleTime);
  } else {
    printf("Cannot calculate idle time\n");
  }
  return 0;
}
