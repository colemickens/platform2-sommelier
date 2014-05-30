// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test evalutes the speed of uploading textures without actually drawing.

#include "base/logging.h"

#include "texturetest.h"
#include "main.h"

namespace glbench {

class TextureUploadTest : public TextureTest {
 public:
  TextureUploadTest() {}
  virtual ~TextureUploadTest() {}
  virtual bool TestFunc(uint64_t iterations);
  virtual const char* Name() const { return "texture_upload"; }
};

bool TextureUploadTest::TestFunc(uint64_t iterations) {
  glGetError();

  for (uint64_t i = 0; i < iterations; ++i) {
    glBindTexture(GL_TEXTURE_2D, textures_[i % kNumberOfTextures]);
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
  }

  return true;
}

TestBase* GetTextureUploadTest() {
  return new TextureUploadTest;
}

} // namespace glbench
