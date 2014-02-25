// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <png.h>
#include <stdio.h>
#include <unistd.h>

#include "base/memory/scoped_ptr.h"
#include "base/file_util.h"

#include "glinterface.h"
#include "md5.h"
#include "png_helper.h"
#include "testbase.h"
#include "utils.h"

DEFINE_bool(save, false, "save images after each test case");
DEFINE_string(outdir, "", "directory to save images");

namespace glbench {

uint64_t TimeTest(TestBase* test, uint64_t iterations) {
    g_main_gl_interface->SwapBuffers();
    glFinish();
    uint64_t time1 = GetUTime();
    if (!test->TestFunc(iterations))
        return ~0;
    glFinish();
    uint64_t time2 = GetUTime();
    return time2 - time1;
}

// Target minimum iteration duration of 1s. This means the final/longest
// iteration is between 1s and 2s and the machine is active for 2s to 4s.
#define MIN_ITERATION_DURATION_US 1000000

// Benchmark some draw commands, by running it many times. We want to measure
// the marginal cost, so we try more and more iterations until we reach the
// minimum specified iteration time.
double Bench(TestBase* test) {
  // Conservatively let machine cool down. Our goal is to sleep at least three
  // times as much (on average) as being active to dissipate heat.
  // TODO(ihf): Investigate if it is necessary to idle even more in the future.
  // In particular cooling down until reaching a temperature threshold.
  const int cool_time = static_cast<int>((10*MIN_ITERATION_DURATION_US)/1.e6);
  // TODO(ihf): Remove this sleep if we have better ways to handle burst/power.
  sleep(cool_time);

  // Do two iterations because initial timings can vary wildly.
  TimeTest(test, 2);

  // We average the times for the last two runs to reduce noise. We could
  // sum up all runs but the initial measurements have high CPU overhead,
  // while the last two runs are both on the order of MIN_ITERATION_DURATION_US.
  uint64_t iterations = 1;
  uint64_t iterations_prev = 0;
  uint64_t time = 0;
  uint64_t time_prev = 0;
  do {
    time = TimeTest(test, iterations);
    if (time > MIN_ITERATION_DURATION_US)
      return (static_cast<double>(time + time_prev) /
              (iterations + iterations_prev));

    time_prev = time;
    iterations_prev = iterations;
    iterations *= 2;
  } while (iterations < (1ULL<<40));

  return 0.0;
}

void SaveImage(const char* name) {
  const int size = g_width * g_height * 4;
  scoped_ptr<char[]> pixels(new char[size]);
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
  scoped_ptr<char[]> pixels(new char[size]);
  glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  MD5Update(&ctx, (unsigned char *)pixels.get(), 4*g_width*g_height);
  MD5Final(digest, &ctx);
}

void RunTest(TestBase* test, const char* testname,
             double coefficient, bool inverse) {
  double value;

  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    value = Bench(test);
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
    if (value == 0.0)
       printf("%-45s=          0   []\n", testname);
    else
      printf("%-45s= %10.2f   [%s]\n", testname,
             coefficient * (inverse ? 1.0 / value : value),
             name_png);
  } else {
    printf("# Error: %s aborted, glGetError returned 0x%02x.\n",
            testname, error);
    // float() in python will happily parse Nan.
    printf("%-45s=        Nan   []\n", testname);
  }
}

bool DrawArraysTestFunc::TestFunc(uint64_t iterations) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
  for (uint64_t i = 0; i < iterations - 1; ++i) {
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  return true;
}


void DrawArraysTestFunc::FillRateTestNormal(const char* name) {
  FillRateTestNormalSubWindow(name, g_width, g_height);
}


void DrawArraysTestFunc::FillRateTestNormalSubWindow(const char* name,
                                                     double width,
                                                     double height)
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


bool DrawElementsTestFunc::TestFunc(uint64_t iterations) {
  glClearColor(0, 1.f, 0, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_SHORT, 0);
  glFlush();
  for (uint64_t i = 0 ; i < iterations - 1; ++i) {
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_SHORT, 0);
  }
  return true;
}

} // namespace glbench
