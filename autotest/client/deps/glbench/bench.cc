// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "main.h"
#include "testbase.h"

uint64_t TimeTest(glbench::TestBase* test, int iter) {
  SwapBuffers();
  glFinish();
  uint64_t time1 = GetUTime();
  test->TestFunc(iter);
  glFinish();
  uint64_t time2 = GetUTime();
  return time2 - time1;
}

// Benchmark some draw commands, by running it many times.
// We want to measure the marginal cost, so we try more and more iterations
// until we get a somewhat linear response (to eliminate constant cost), and we
// do a linear regression on a few samples.
bool Bench(glbench::TestBase* test, float *slope, int64_t *bias) {
  // Do one iteration in case the driver needs to set up states.
  if (TimeTest(test, 1) > MAX_ITERATION_DURATION_MS)
    return false;
  int64_t count = 0;
  int64_t sum_x = 0;
  int64_t sum_y = 0;
  int64_t sum_xy = 0;
  int64_t sum_x2 = 0;
  uint64_t last_time = 0;
  bool do_count = false;
  uint64_t iter;
  for (iter = 8; iter < 1<<30; iter *= 2) {
    uint64_t time = TimeTest(test, iter);
    if (last_time > 0 && (time > last_time * 1.8))
      do_count = true;
    last_time = time;
    if (do_count) {
      ++count;
      sum_x += iter;
      sum_y += time;
      sum_xy += iter * time;
      sum_x2 += iter * iter;
    }
    if ((time >= 500000 && count > 4))
      break;
  }
  if (count < 2) {
    *slope = 0.f;
    *bias = 0;
  }
  *slope = static_cast<float>(sum_x * sum_y - count * sum_xy) /
    (sum_x * sum_x - count * sum_x2);
  *bias = (sum_x * sum_xy - sum_x2 * sum_y) / (sum_x * sum_x - count * sum_x2);
  return true;
}
