// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "main.h"
#include "testbase.h"
#include "utils.h"

namespace glbench {


class FillRateTest : public DrawArraysTestFunc {
 public:
  FillRateTest() {}
  virtual ~FillRateTest() {}
  virtual bool Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(FillRateTest);
};


bool FillRateTest::Run() {
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLfloat buffer_vertex[8] = {
    -1.f, -1.f,
    1.f,  -1.f,
    -1.f, 1.f,
    1.f,  1.f,
  };
  GLfloat buffer_texture[8] = {
    0.f, 0.f,
    1.f, 0.f,
    0.f, 1.f,
    1.f, 1.f,
  };
  glEnableClientState(GL_VERTEX_ARRAY);

  GLuint vbo_vertex = SetupVBO(GL_ARRAY_BUFFER,
                               sizeof(buffer_vertex), buffer_vertex);
  glVertexPointer(2, GL_FLOAT, 0, 0);

  GLuint vbo_texture = SetupVBO(GL_ARRAY_BUFFER,
                                sizeof(buffer_texture), buffer_texture);
  glTexCoordPointer(2, GL_FLOAT, 0, 0);

  glColor4f(1.f, 0.f, 0.f, 1.f);
  FillRateTestNormal("fill_solid");
  FillRateTestBlendDepth("fill_solid");

  glColor4f(1.f, 1.f, 1.f, 1.f);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnable(GL_TEXTURE_2D);

  GLuint texture = SetupTexture(9);
  FillRateTestNormal("fill_tex_nearest");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  FillRateTestNormal("fill_tex_bilinear");

  // lod = 0.5
  glScalef(0.7071f, 0.7071f, 1.f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_NEAREST);
  FillRateTestNormalSubWindow("fill_tex_trilinear_nearest_05",
                              0.7071f * g_width, 0.7071f * g_height);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_05",
                              0.7071f * g_width, 0.7071f * g_height);

  // lod = 0.4
  glLoadIdentity();
  glScalef(0.758f, 0.758f, 1.f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_04",
                              0.758f * g_width, 0.758f * g_height);

  // lod = 0.1
  glLoadIdentity();
  glScalef(0.933f, 0.933f, 1.f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_01",
                              0.933f * g_width, 0.933f * g_height);

  glDeleteBuffers(1, &vbo_vertex);
  glDeleteBuffers(1, &vbo_texture);
  glDeleteTextures(1, &texture);

  return true;
}

TestBase* GetFillRateTest() {
  return new FillRateTest;
}

} // namespace glbench
