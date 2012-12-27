// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test evaluates the speed of updating a single texture and using it to
// draw after each upload.

#include "base/logging.h"

#include "texturetest.h"
#include "main.h"

namespace glbench {

class TextureUpdateTest : public TextureTest {
 public:
  TextureUpdateTest() {}
  virtual ~TextureUpdateTest() {}
  virtual bool TestFunc(int iter);
  virtual const char* Name() const { return "texture_update"; }
};

bool TextureUpdateTest::TestFunc(int iter) {
  glGetError();

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
  for (int i = 0; i < iter - 1; ++i) {
    switch (flavor_) {
      case TEX_IMAGE:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_,
                     0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     pixels_[i % kNumberOfTextures].get());
        break;
      case TEX_SUBIMAGE:
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE,
                        pixels_[i % kNumberOfTextures].get());
        break;
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  return true;
}

TestBase* GetTextureUpdateTest() {
  return new TextureUpdateTest;
}

} // namespace glbench
