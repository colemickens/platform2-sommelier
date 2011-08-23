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
  virtual const char* Name() const { return "fill_rate"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FillRateTest);
};

#if defined(I915_WORKAROUND)
#define V1 "gl_TexCoord[0]"
#else
#define V1 "v1"
#endif

const char* kVertexShader1 =
    "attribute vec4 position;"
    "void main() {"
    "  gl_Position = position;"
    "}";

const char* kFragmentShader1 =
    "uniform vec4 color;"
    "void main() {"
    "  gl_FragColor = color;"
    "}";


const char* kVertexShader2 =
    "attribute vec4 position;"
    "attribute vec4 texcoord;"
    "uniform float scale;"
    "varying vec4 v1;"
    "void main() {"
    "  gl_Position = position * vec4(scale, scale, 1., 1.);"
    "  " V1 " = texcoord;"
    "}";

const char* kFragmentShader2 =
    "uniform sampler2D texture;"
    "varying vec4 v1;"
    "void main() {"
    "  gl_FragColor = texture2D(texture, " V1 ".xy);"
    "}";

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

  GLuint vbo_vertex = SetupVBO(GL_ARRAY_BUFFER,
                               sizeof(buffer_vertex), buffer_vertex);
  GLuint program = InitShaderProgram(kVertexShader1, kFragmentShader1);
  GLint position_attribute = glGetAttribLocation(program, "position");
  glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(position_attribute);

  GLint color_uniform = glGetUniformLocation(program, "color");
  const GLfloat red[4] = {1.f, 0.f, 0.f, 1.f};
  glUniform4fv(color_uniform, 1, red);

  FillRateTestNormal("fill_solid");
  FillRateTestBlendDepth("fill_solid");

  glDeleteProgram(program);

  program = InitShaderProgram(kVertexShader2, kFragmentShader2);
  position_attribute = glGetAttribLocation(program, "position");
  // Reusing vbo_vertex buffer from the previous test.
  glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(position_attribute);

  GLuint vbo_texture = SetupVBO(GL_ARRAY_BUFFER,
                                sizeof(buffer_texture), buffer_texture);
  GLuint texcoord_attribute = glGetAttribLocation(program, "texcoord");
  glVertexAttribPointer(texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(texcoord_attribute);

  GLuint texture = SetupTexture(9);

  GLuint texture_uniform = glGetUniformLocation(program, "texture");
  glUniform1i(texture_uniform, 0);

  GLuint scale_uinform = glGetUniformLocation(program, "scale");
  glUniform1f(scale_uinform, 1.f);

  FillRateTestNormal("fill_tex_nearest");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  FillRateTestNormal("fill_tex_bilinear");

  // lod = 0.5
  glUniform1f(scale_uinform, 0.7071f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_05",
                              0.7071f * g_width, 0.7071f * g_height);

  // lod = 0.4
  glUniform1f(scale_uinform, 0.758f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_04",
                              0.758f * g_width, 0.758f * g_height);

  // lod = 0.1
  glUniform1f(scale_uinform, 0.933f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_01",
                              0.933f * g_width, 0.933f * g_height);

  glDeleteProgram(program);
  glDeleteBuffers(1, &vbo_vertex);
  glDeleteBuffers(1, &vbo_texture);
  glDeleteTextures(1, &texture);

  return true;
}

TestBase* GetFillRateTest() {
  return new FillRateTest;
}

} // namespace glbench
