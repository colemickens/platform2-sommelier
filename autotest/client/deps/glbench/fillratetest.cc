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

class FboFillRateTest : public DrawArraysTestFunc {
 public:
  FboFillRateTest() {}
  virtual ~FboFillRateTest() {}
  virtual bool Run();
  virtual const char* Name() const { return "fbo_fill_rate"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FboFillRateTest);
};

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
    "  v1 = texcoord;"
    "}";

const char* kFragmentShader2 =
    "uniform sampler2D texture;"
    "varying vec4 v1;"
    "void main() {"
    "  gl_FragColor = texture2D(texture, v1.xy);"
    "}";

const GLfloat buffer_vertex[8] = {
  -1.f, -1.f,
  1.f,  -1.f,
  -1.f, 1.f,
  1.f,  1.f,
};

const GLfloat buffer_texture[8] = {
  0.f, 0.f,
  1.f, 0.f,
  0.f, 1.f,
  1.f, 1.f,
};

const GLfloat red[4] = {1.f, 0.f, 0.f, 1.f};


bool FillRateTest::Run() {
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLuint vbo_vertex = SetupVBO(GL_ARRAY_BUFFER,
                               sizeof(buffer_vertex), buffer_vertex);
  GLuint program = InitShaderProgram(kVertexShader1, kFragmentShader1);
  GLint position_attribute = glGetAttribLocation(program, "position");
  glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(position_attribute);

  GLint color_uniform = glGetUniformLocation(program, "color");
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

  // Get a fractal looking source texture of size 512x512 and full levels
  // of detail.
  GLuint texture = SetupTexture(9, 9);

  GLuint texture_uniform = glGetUniformLocation(program, "texture");
  glUniform1i(texture_uniform, 0);

  GLuint scale_uniform = glGetUniformLocation(program, "scale");
  glUniform1f(scale_uniform, 1.f);

  FillRateTestNormal("fill_tex_nearest");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  FillRateTestNormal("fill_tex_bilinear");

  // lod = 0.5
  glUniform1f(scale_uniform, 0.7071f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_05",
                              0.7071f * g_width, 0.7071f * g_height);

  // lod = 0.4
  glUniform1f(scale_uniform, 0.758f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormalSubWindow("fill_tex_trilinear_linear_04",
                              0.758f * g_width, 0.758f * g_height);

  // lod = 0.1
  glUniform1f(scale_uniform, 0.933f);
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

bool FboFillRateTest::Run() {
  char name[256];
  int log_size = 0;
  CHECK(!glGetError());
  GLuint vbo_vertex = SetupVBO(GL_ARRAY_BUFFER,
                               sizeof(buffer_vertex), buffer_vertex);
  GLuint program = InitShaderProgram(kVertexShader2, kFragmentShader2);
  GLint position_attribute = glGetAttribLocation(program, "position");
  glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(position_attribute);
  GLuint vbo_texture = SetupVBO(GL_ARRAY_BUFFER,
                                sizeof(buffer_texture), buffer_texture);
  GLuint texcoord_attribute = glGetAttribLocation(program, "texcoord");
  glVertexAttribPointer(texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(texcoord_attribute);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  CHECK(!glGetError());

  // We don't care for tiny texture sizes.
  for (int size = 32; size <= g_max_texture_size; size *= 2) {
    // In hasty mode only do 512x512 sized problems.
    if (g_hasty && size != 512)
      continue;
    sprintf(name, "fbofill_tex_bilinear_%d", size);

    // Setup texture for FBO.
    GLuint destination_texture = 0;
    glGenTextures(1, &destination_texture);
    glBindTexture(GL_TEXTURE_2D, destination_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CHECK(!glGetError());

    // Setup Framebuffer.
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, destination_texture, 0);
    CHECK(!glGetError());

    // Attach texture and check for completeness.
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    CHECK(status == GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, size, size);

    // Get a fractal looking source texture of size size*size and only the
    // base level of detail.
    GLuint source_texture = SetupTexture(log_size, 0);
    GLuint texture_uniform = glGetUniformLocation(program, "texture");
    glUniform1i(texture_uniform, 0);
    GLuint scale_uniform = glGetUniformLocation(program, "scale");
    glUniform1f(scale_uniform, 1.f);

    // Run the benchmark, save the images if desired.
    FillRateTestNormalSubWindow(name, size, size);

    // Clean up for this loop.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &source_texture);
    glDeleteTextures(1, &destination_texture);
    CHECK(!glGetError());

    log_size++;
  }
  // Clean up invariants.
  glDeleteProgram(program);
  glDeleteBuffers(1, &vbo_vertex);
  glDeleteBuffers(1, &vbo_texture);
  // Just in case restore the viewport for all other tests.
  glViewport(0, 0, g_width, g_height);

  return true;
}

TestBase* GetFillRateTest() {
  return new FillRateTest;
}

TestBase* GetFboFillRateTest() {
  return new FboFillRateTest;
}

} // namespace glbench
