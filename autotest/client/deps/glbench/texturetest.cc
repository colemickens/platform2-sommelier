// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "texturetest.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace glbench {

namespace {

// Vertex and fragment shader code.
const char* kVertexShader =
    "attribute vec4 c1;"
    "attribute vec4 c2;"
    "varying vec4 v1;"
    "void main() {"
    "  gl_Position = c1;"
    "  v1 = c2;"
    "}";

const char* kFragmentShader =
    "varying vec4 v1;"
    "uniform sampler2D texture;"
    "void main() {"
    "  gl_FragColor = texture2D(texture, v1.xy);"
    "}";

}  // namespace

bool TextureTest::Run() {
  // Two triangles that form one pixel at 0, 0.
  const GLfloat kVertices[8] = {
    0.f, 0.f,
    2.f / g_width, 0.f,
    0.f, 2.f / g_height,
    2.f / g_width, 2.f / g_height,
  };
  const GLfloat kTexCoords[8] = {
    0.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 0.f,
  };

  program_ = InitShaderProgram(kVertexShader, kFragmentShader);

  int attr1 = glGetAttribLocation(program_, "c1");
  glVertexAttribPointer(attr1, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
  glEnableVertexAttribArray(attr1);

  int attr2 = glGetAttribLocation(program_, "c2");
  glVertexAttribPointer(attr2, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
  glEnableVertexAttribArray(attr2);

  int texture_sampler = glGetUniformLocation(program_, "texture");
  glUniform1i(texture_sampler, 0);
  glActiveTexture(GL_TEXTURE0);

  glGenTextures(kNumberOfTextures, textures_);
  for (int i = 0; i < kNumberOfTextures; ++i) {
    glBindTexture(GL_TEXTURE_2D, textures_[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  // Types of textures, and corresponding name strings for logging.
  UpdateFlavor kFlavors[] = { TEX_IMAGE, TEX_SUBIMAGE };
  const std::string kFlavorNames[] = { "teximage2d", "texsubimage2d" };

  for (unsigned int f = 0; f < arraysize(kFlavors); f++) {
    flavor_ = kFlavors[f];
    int sizes[] = {32, 128, 256, 512, 768, 1024, 1536, 2048};
    for (unsigned int j = 0; j < arraysize(sizes); j++) {
      // In hasty mode only do at most 512x512 sized problems.
      if (g_hasty && sizes[j] > 512)
        continue;
      std::string name = std::string(Name()) + "_" + kFlavorNames[f] + "_" +
          base::IntToString(sizes[j]);
      width_ = height_ = sizes[j];
      for (int i = 0; i < kNumberOfTextures; ++i) {
        pixels_[i].reset(new char[width_ * height_]);
        memset(pixels_[i].get(), 255, width_ * height_);

        //For NPOT texture we must set GL_TEXTURE_WRAP as GL_CLAMP_TO_EDGE
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        if (glGetError() != 0) {
          printf("# Error: Failed to allocate %dx%d texture.\n", width_,
                 height_);
        }
        if (IS_NOT_POWER_OF_2(width_) || IS_NOT_POWER_OF_2(height_)) {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
      }
      RunTest(this, name.c_str(), width_ * height_, g_width, g_height, true);
      GLenum error = glGetError();
      if (error != GL_NO_ERROR) {
        printf("# GL error code %d after RunTest() with %dx%d texture.\n",
               error, width_, height_);
      }
    }
  }
  for (int i = 0; i < kNumberOfTextures; ++i)
    pixels_[i].reset();

  glDeleteTextures(kNumberOfTextures, textures_);
  glDeleteProgram(program_);
  return true;
}

} // namespace glbench
