// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "main.h"
#include "testbase.h"


namespace glbench {


class SwapTest : public TestBase {
 public:
  SwapTest() {}
  virtual ~SwapTest() {}
  virtual bool TestFunc(int iter);
  virtual bool Run();
  virtual const char* Name() const { return "swap"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SwapTest);
};


bool SwapTest::TestFunc(int iter) {
  for (int i = 0 ; i < iter; ++i) {
    SwapBuffers();
  }
  return true;
}


bool SwapTest::Run() {
  RunTest(this, "us_swap_swap", 1.f, false);
  return true;
}


TestBase* GetSwapTest() {
  return new SwapTest;
}

} // namespace glbench
