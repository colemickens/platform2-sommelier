// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "testbase.h"

namespace glbench {

uint64_t TimeTest(TestBase* test, int iter) {
  SwapBuffers();
  glFinish();
  uint64_t time1 = GetUTime();
  if (!test->TestFunc(iter))
    return ~0;
  glFinish();
  uint64_t time2 = GetUTime();
  return time2 - time1;
}

#define MAX_ITERATION_DURATION_MS 100000

// Benchmark some draw commands, by running it many times.
// We want to measure the marginal cost, so we try more and more iterations
// until we get a somewhat linear response (to eliminate constant cost), and we
// do a linear regression on a few samples.
bool Bench(TestBase* test, float *slope, int64_t *bias) {
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

void RunTest(TestBase* test, const char *name,
             float coefficient, bool inverse) {
  float slope;
  int64_t bias;

  GLenum err = glGetError();
  if (err != 0) {
    printf("# %s failed, glGetError returned 0x%x.\n", name, err);
    // float() in python will happily parse Nan.
    printf("%s: Nan\n", name);
  } else {
    if (Bench(test, &slope, &bias)) {
      printf("%s: %g\n", name, coefficient * (inverse ? 1.f / slope : slope));
    } else {
      printf("# %s is too slow, returning zero.\n", name);
      printf("%s: 0\n", name);
    }
  }
}

bool DrawArraysTestFunc::TestFunc(int iter) {
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
  for (int i = 0; i < iter-1; ++i) {
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  return true;
}


void DrawArraysTestFunc::FillRateTestNormal(const char* name) {
  FillRateTestNormalSubWindow(name, g_width, g_height);
}


void DrawArraysTestFunc::FillRateTestNormalSubWindow(const char* name,
                                                     float width, float height)
{
  const int buffer_len = 64;
  char buffer[buffer_len];
  snprintf(buffer, buffer_len, "mpixels_sec_%s", name);
  RunTest(this, buffer, width * height, true);
}


#if defined(USE_OPENGL)
void DrawArraysTestFunc::FillRateTestBlendDepth(const char *name) {
  const int buffer_len = 64;
  char buffer[buffer_len];

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_blended", name);
  RunTest(this, buffer, g_width * g_height, true);
  glDisable(GL_BLEND);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_NOTEQUAL);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_depth_neq", name);
  RunTest(this, buffer, g_width * g_height, true);
  glDepthFunc(GL_NEVER);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_depth_never", name);
  RunTest(this, buffer, g_width * g_height, true);
  glDisable(GL_DEPTH_TEST);
}
#endif


bool DrawElementsTestFunc::TestFunc(int iter) {
  glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, 0);
  glFlush();
  for (int i = 0 ; i < iter-1; ++i) {
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, 0);
  }
  return true;
}

} // namespace glbench
