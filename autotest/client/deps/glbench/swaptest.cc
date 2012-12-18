// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"

#include "main.h"
#include "testbase.h"
#include "utils.h"


namespace glbench {


class SwapTest : public TestBase {
 public:
  SwapTest() : index_buffer_object_(0),
               vertex_buffer_object_(0),
               num_indices_(0),
               shader_program_(0),
               attribute_index_(0) {}
  virtual ~SwapTest() {}
  virtual bool TestFunc(int iter);
  virtual bool Run();
  virtual const char* Name() const { return "swap"; }

 private:
  // For calling some GL operations before SwapBuffers().
  void SetupGLRendering();
  void RenderGLSimple();
  void CleanupGLRendering();

  // For GL rendering.
  GLuint index_buffer_object_;
  GLuint vertex_buffer_object_;
  GLsizei num_indices_;
  GLuint shader_program_;
  GLint attribute_index_;

  // Callback for GL rendering function to be run before calling SwapBuffers().
  base::Callback<void(void)> render_func_;

  DISALLOW_COPY_AND_ASSIGN(SwapTest);
};


namespace {

// Basic shader code.
const char* kVertexShader =
    "attribute vec4 c;"
    "void main() {"
    "  gl_Position = c;"
    "}";

const char* kFragmentShader =
    "uniform vec4 color;"
    "void main() {"
    "  gl_FragColor = color;"
    "}";

// Vertex arrays used to draw a diamond.
const GLfloat kVertices[] = { 1.0, 0.0,
                              0.0, -1.0,
                              -1.0, 0.0,
                              0.0, 1.0 };
const GLushort kIndices[] = { 0, 1, 2,
                              0, 2, 3 };

}  // namespace


bool SwapTest::TestFunc(int iter) {
  for (int i = 0 ; i < iter; ++i) {
    if (!render_func_.is_null())
      render_func_.Run();
    SwapBuffers();
  }
  return true;
}

void SwapTest::SetupGLRendering() {
  vertex_buffer_object_ =
      SetupVBO(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices);

  shader_program_ = InitShaderProgram(kVertexShader, kFragmentShader);
  attribute_index_ = glGetAttribLocation(shader_program_, "c");
  glVertexAttribPointer(attribute_index_, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index_);

  GLint color_uniform = glGetUniformLocation(shader_program_, "color");

  const GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  glUniform4fv(color_uniform, 1, white);

  num_indices_ = arraysize(kIndices);
  index_buffer_object_ =
      SetupVBO(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices);
}

void SwapTest::CleanupGLRendering() {
  glDisableVertexAttribArray(attribute_index_);
  glDeleteProgram(shader_program_);
  glDeleteBuffers(1, &index_buffer_object_);
  glDeleteBuffers(1, &vertex_buffer_object_);
}


bool SwapTest::Run() {
  // Run buffer swapping only.
  render_func_.Reset();
  RunTest(this, "us_swap_swap", 1.f, false);

  // Run buffer swapping with simple GL commands.
  SetupGLRendering();
  render_func_ = base::Bind(&SwapTest::RenderGLSimple, base::Unretained(this));
  RunTest(this, "us_swap_swap_glsimple", 1.f, false);
  CleanupGLRendering();

  // TODO(crosbug.com/36746): Run buffer swapping with complex GL commands.
  return true;
}

void SwapTest::RenderGLSimple() {
  glDrawElements(GL_TRIANGLES, num_indices_, GL_UNSIGNED_SHORT, 0);
}

TestBase* GetSwapTest() {
  return new SwapTest;
}

} // namespace glbench
