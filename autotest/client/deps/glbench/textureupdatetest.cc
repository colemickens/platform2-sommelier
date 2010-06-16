// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

#include "main.h"
#include "testbase.h"
#include "utils.h"

namespace glbench {

static const int kNumberOfTextures = 8;

class TextureUpdateTest : public TestBase {
 public:
  TextureUpdateTest() {}
  virtual ~TextureUpdateTest() {}
  bool TestFunc(int iter);
  virtual bool Run();
  virtual const char* Name() const { return "texture_update"; }

  enum UpdateFlavor {
    TEX_IMAGE,
    TEX_SUBIMAGE
  };

 private:
  GLuint width_;
  GLuint height_;
  GLuint program_;
  int texsize_;
  scoped_array<char> pixels_[kNumberOfTextures];
  UpdateFlavor flavor_;
  DISALLOW_COPY_AND_ASSIGN(TextureUpdateTest);
};

#if defined(I915_WORKAROUND)
#define V1 "gl_TexCoord[0]"
#else
#define V1 "v1"
#endif

static const char* kVertexShader =
    "attribute vec4 c1;"
    "attribute vec4 c2;"
    "varying vec4 v1;"
    "void main() {"
    "  gl_Position = c1;"
       V1 "= c2;"
    "}";

static const char* kFragmentShader =
    "varying vec4 v1;"
    "uniform sampler2D texture;"
    "void main() {"
    "  gl_FragColor = texture2D(texture, " V1 ".xy);"
    "}";

bool TextureUpdateTest::TestFunc(int iter) {
  glGetError();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_,
               0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
  if (glGetError() != 0) {
    printf("# Failed to allocate %dx%d texture.\n", width_, height_);
    return false;
  }

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
  for (int i = 0; i < iter-1; ++i) {
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

bool TextureUpdateTest::Run() {
  // Two triangles that form one pixel at 0, 0.
  GLfloat vertices[8] = {
    0.f, 0.f,
    2.f / g_width, 0.f,
    0.f, 2.f / g_height,
    2.f / g_width, 2.f / g_height,
  };
  GLfloat texcoords[8] = {
    0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f,
  };

  program_ = InitShaderProgram(kVertexShader, kFragmentShader);

  int attr1 = glGetAttribLocation(program_, "c1");
  glVertexAttribPointer(attr1, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  glEnableVertexAttribArray(attr1);

  int attr2 = glGetAttribLocation(program_, "c2");
  glVertexAttribPointer(attr2, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
  glEnableVertexAttribArray(attr2);

  int texture_sampler = glGetUniformLocation(program_, "texture");
  glUniform1i(texture_sampler, 0);
  glActiveTexture(GL_TEXTURE0);

  GLuint texname;
  glGenTextures(1, &texname);
  glBindTexture(GL_TEXTURE_2D, texname);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


  UpdateFlavor flavors[] = {TEX_IMAGE, TEX_SUBIMAGE};
  const std::string flavor_names[] = {
    "texture_update_teximage2d", "texture_update_texsubimage2d"
  };
  for (unsigned int f = 0; f < arraysize(flavors); f++) {
    flavor_ = flavors[f];
    int sizes[] = {32, 128, 256, 512, 768, 1024, 1536, 2048};
    for (unsigned int i = 0; i < arraysize(sizes); i++) {
      std::string name = "mtexel_sec_" + flavor_names[f] + "_" +
          IntToString(sizes[i]);
      width_ = height_ = sizes[i];
      for (int i = 0; i < kNumberOfTextures; ++i) {
        pixels_[i].reset(new char[width_ * height_]);
        memset(pixels_[i].get(), 255, width_ * height_);
      }
      RunTest(this, name.c_str(), width_ * height_, true);
    }
  }

  glDeleteTextures(1, &texname);
  glDeleteProgram(program_);
  return true;
}

TestBase* GetTextureUpdateTest() {
  return new TextureUpdateTest;
}

} // namespace glbench
