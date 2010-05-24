// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_TESTBASE_H_
#define BENCH_GL_TESTBASE_H_

#include "base/basictypes.h"

#include "main.h"


namespace glbench {

class TestBase;

// Runs test->TestFunc() passing it sequential powers of two (8, 16, 32,...)
// recording time it took.  The data is then fitted linearly, obtaining slope
// and bias such that:
//   time it took to run x iterations = slope * x + bias
// Returns false if one iteration of the test takes longer than
// MAX_ITERATION_LENGTH_MS.  The test is then assumed too slow to provide
// meaningful results.
bool Bench(TestBase* test, float *slope, int64_t *bias);

// Runs Bench on an instance of TestBase and prints out results.
// 
// coefficient is multiplied (if inverse is false) or divided (if inverse is
// true) by the slope and the result is printed.
//
// Examples:
//   coefficient = width * height (measured in pixels), inverse = true
//       returns the throughput in megapixels per second;
//
//   coefficient = 1, inverse = false
//       returns number of operations per second.
void RunTest(TestBase* test, const char *name, float coefficient, bool inverse);


class TestBase {
 public:
  virtual ~TestBase() {}
  // Runs the test case n times.
  virtual bool TestFunc(int n) = 0;
  // Main entry point into the test.
  virtual bool Run() = 0;
};

// Helper class to time glDrawArrays.
class DrawArraysTestFunc : public TestBase {
 public:
  virtual ~DrawArraysTestFunc() {}
  virtual bool TestFunc(int);

  // Runs the test and reports results in mpixels per second, assuming each
  // iteration updates the whole window (its size is g_width by g_height).
  void FillRateTestNormal(const char* name);
  // Runs the test and reports results in mpixels per second, assuming each
  // iteration updates a window of width by height pixels.
  void FillRateTestNormalSubWindow(const char* name, float width, float height);
#if defined(USE_OPENGL)
  // Runs the test three times: with blending on; with depth test enabled and
  // depth function of GL_NOTEQUAL; with depth function GL_NEVER.  Results are
  // reported as in FillRateTestNormal.
  void FillRateTestBlendDepth(const char *name);
#endif
};

// Helper class to time glDrawElements.
class DrawElementsTestFunc : public TestBase {
 public:
  DrawElementsTestFunc() : count_(0) {}
  virtual ~DrawElementsTestFunc() {}
  virtual bool TestFunc(int);

 protected:
  // Passed to glDrawElements.
  GLsizei count_;
};

} // namespace glbench

#endif // BENCH_GL_TESTBASE_H_
