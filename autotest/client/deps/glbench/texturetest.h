// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_TEXTURETEST_H_
#define BENCH_GL_TEXTURETEST_H_

#include "base/memory/scoped_ptr.h"

#include "testbase.h"
#include "utils.h"

namespace glbench {

namespace {

const int kNumberOfTextures = 8;

}  // namespace

class TextureTest : public TestBase {
 public:
  TextureTest() {}
  virtual ~TextureTest() {}
  virtual bool TestFunc(int iter) = 0;
  virtual bool Run();
  virtual const char* Name() const = 0;

  enum UpdateFlavor {
    TEX_IMAGE,
    TEX_SUBIMAGE
  };

 protected:
  GLuint width_;
  GLuint height_;
  GLuint program_;
  int texsize_;
  scoped_array<char> pixels_[kNumberOfTextures];
  GLuint textures_[kNumberOfTextures];
  UpdateFlavor flavor_;
  DISALLOW_COPY_AND_ASSIGN(TextureTest);
};

} // namespace glbench

#endif  // BENCH_GL_TEXTURETEST_H_
