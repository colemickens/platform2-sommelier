// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <stdio.h>
#include <png.h>

#include "base/memory/scoped_ptr.h"
#include "base/file_util.h"

#include "png_helper.h"
#include "md5.h"
#include "testbase.h"
#include "utils.h"

DEFINE_bool(save, false, "save images after each test case");
DEFINE_string(outdir, "", "directory to save images");

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

// Maximum iteration time of 0.1s for regression approach
#define MAX_ITERATION_DURATION_US 100000

// Benchmark some draw commands, by running it many times.
// We want to measure the marginal cost, so we try more and more
// iterations until we get a somewhat linear response (to
// eliminate constant cost), and we do a linear regression on
// a few samples.
bool Bench(TestBase* test, float *slope, int64_t *bias) {
  // Do two iterations because initial timings can vary wildly.
  TimeTest(test, 1);
  uint64_t initial_time = TimeTest(test, 1);
  if (initial_time > MAX_ITERATION_DURATION_US) {
    // The test is too slow to do the regression,
    // so just return a single result.
    *slope = static_cast<float>(TimeTest(test, 1));
    *bias = 0;
    return true;
  }

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
    if (last_time > 0 && (time > last_time * 1.8)) {
      do_count = true;
    }
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

void SaveImage(const char* name) {
  const int size = g_width * g_height * 4;
  scoped_array<char> pixels(new char[size]);
  glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  // I really think we want to use outdir as a straight argument
  FilePath dirname = FilePath(FLAGS_outdir);
  file_util::CreateDirectory(dirname);
  FilePath filename = dirname.Append(name);
  write_png_file(filename.value().c_str(),
                 pixels.get(), g_width, g_height);
}

void ComputeMD5(unsigned char digest[16]) {
  MD5Context ctx;
  MD5Init(&ctx);
  const int size = g_width * g_height * 4;
  scoped_array<char> pixels(new char[size]);
  glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  MD5Update(&ctx, (unsigned char *)pixels.get(), 4*g_width*g_height);
  MD5Final(digest, &ctx);
}

void RunTest(TestBase* test, const char* testname,
             float coefficient, bool inverse) {
  float slope;
  int64_t bias;

  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    bool status = Bench(test, &slope, &bias);
    if (status) {
      // save as png with MD5 as hex string attached
      char          pixmd5[33];
      unsigned char d[16];
      ComputeMD5(d);
      // translate to hexadecimal ASCII of MD5
      sprintf(pixmd5,
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        d[ 0],d[ 1],d[ 2],d[ 3],d[ 4],d[ 5],d[ 6],d[ 7],
        d[ 8],d[ 9],d[10],d[11],d[12],d[13],d[14],d[15]);
      char name_png[512];
      sprintf(name_png, "%s.pixmd5-%s.png", testname, pixmd5);

      if (FLAGS_save)
        SaveImage(name_png);

      // TODO(ihf) adjust string length based on longest test name
      int length = strlen(testname);
      if (length > 45)
          printf("# Warning: adjust string formatting to length = %d\n",
                 length);
      printf("%-45s= %10.2f   [%s]\n",
             testname,
             coefficient * (inverse ? 1.f / slope : slope),
             name_png);
    } else {
      printf("# Warning: %s scales non-linearly, returning zero.\n",
             testname);
      printf("%-45s=          0   []\n", testname);
    }
  } else {
    printf("# Error: %s aborted, glGetError returned 0x%02x.\n",
            testname, error);
    // float() in python will happily parse Nan.
    printf("%-45s=        Nan   []\n", testname);
  }
}

bool DrawArraysTestFunc::TestFunc(int iter) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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


void DrawArraysTestFunc::FillRateTestBlendDepth(const char *name) {
  const int buffer_len = 64;
  char buffer[buffer_len];

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_blended", name);
  RunTest(this, buffer, g_width * g_height, true);
  glDisable(GL_BLEND);

  // We are relying on the default depth clear value of 1 here.
  // Fragments should have depth 0.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_NOTEQUAL);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_depth_neq", name);
  RunTest(this, buffer, g_width * g_height, true);

  // The DrawArrays call invoked by this test shouldn't render anything
  // because every fragment will fail the depth test.  Therefore we
  // should see the clear color.
  glDepthFunc(GL_NEVER);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_depth_never", name);
  RunTest(this, buffer, g_width * g_height, true);
  glDisable(GL_DEPTH_TEST);
}


bool DrawElementsTestFunc::TestFunc(int iter) {
  glClearColor(0, 1.f, 0, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_SHORT, 0);
  glFlush();
  for (int i = 0 ; i < iter-1; ++i) {
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_SHORT, 0);
  }
  return true;
}

} // namespace glbench
