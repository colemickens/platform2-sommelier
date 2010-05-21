// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"

#include "main.h"
#include "testbase.h"


namespace glbench {


class ReadPixelTest : public TestBase {
 public:
  ReadPixelTest() : pixels_(NULL) {}
  virtual ~ReadPixelTest() {}
  virtual bool TestFunc(int iter);
  virtual bool Run();

 private:
  void* pixels_;
  DISALLOW_COPY_AND_ASSIGN(ReadPixelTest);
};


bool ReadPixelTest::TestFunc(int iter) {
  glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_);
  CHECK(glGetError() == 0);
  for (int i = 0; i < iter; i++)
    glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_);
  return true;
}


bool ReadPixelTest::Run() {
  // One GL_RGBA pixel takes 4 bytes.
  const int row_size = g_width * 4;
  // Default GL_PACK_ALIGNMENT is 4, round up pixel row size to multiple of 4.
  // This is a no-op because row_size is already divisible by 4.
  // One is added so that we can test reads into unaligned location.
  scoped_array<char> buf(new char[((row_size + 3) & ~3) * g_height + 1]);
  pixels_ = buf.get();
  RunTest(this, "mpixels_sec_pixel_read", g_width * g_height, true);

  // Reducing GL_PACK_ALIGNMENT can only make rows smaller.  No need to
  // reallocate the buffer.
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  RunTest(this, "mpixels_sec_pixel_read_2", g_width * g_height, true);

  pixels_ = static_cast<void*>(buf.get() + 1);
  RunTest(this, "mpixels_sec_pixel_read_3", g_width * g_height, true);

  return true;
}


TestBase* GetReadPixelTest() {
  return new ReadPixelTest;
}


} // namespace glbench
