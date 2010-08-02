// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "main.h"
#include "testbase.h"
#include "utils.h"


namespace glbench {


class VaryingsAndDdxyShaderTest : public DrawElementsTestFunc {
 public:
  VaryingsAndDdxyShaderTest() {}
  virtual ~VaryingsAndDdxyShaderTest() {}
  virtual bool Run();
  virtual const char* Name() const { return "varyings_ddx_shader"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(VaryingsAndDdxyShaderTest);
};

#if defined(I915_WORKAROUND)
#define V1 "gl_TexCoord[0]"
#define V2 "gl_TexCoord[1]"
#define V3 "gl_TexCoord[2]"
#define V4 "gl_TexCoord[3]"
#define V5 "gl_TexCoord[4]"
#define V6 "gl_TexCoord[5]"
#define V7 "gl_TexCoord[6]"
#define V8 "gl_TexCoord[7]"
#else
#define V1 "v1"
#define V2 "v2"
#define V3 "v3"
#define V4 "v4"
#define V5 "v5"
#define V6 "v6"
#define V7 "v7"
#define V8 "v8"
#endif


const char *vertex_shader_1_varying =
"attribute vec4 c;"
"varying vec4 v1;"
"void main() {"
"  gl_Position = c;"
   V1 "= c;"
"}";

const char *vertex_shader_2_varying =
"attribute vec4 c;"
"varying vec4 v1;"
"varying vec4 v2;"
"void main() {"
"  gl_Position = c;"
   V1 "=" V2 "= c/2.;"
"}";

const char *vertex_shader_4_varying =
"attribute vec4 c;"
"varying vec4 v1;"
"varying vec4 v2;"
"varying vec4 v3;"
"varying vec4 v4;"
"void main() {"
"  gl_Position = c;"
   V1 "=" V2 "=" V3 "=" V4 "= c/4.;"
"}";

const char *vertex_shader_8_varying =
"attribute vec4 c;"
"varying vec4 v1;"
"varying vec4 v2;"
"varying vec4 v3;"
"varying vec4 v4;"
"varying vec4 v5;"
"varying vec4 v6;"
"varying vec4 v7;"
"varying vec4 v8;"
"void main() {"
"  gl_Position = c;"
   V1 "=" V2 "=" V3 "=" V4 "=" V5 "=" V6 "=" V7 "=" V8 "= c/8.;"
"}";

const char *fragment_shader_1_varying =
"varying vec4 v1;"
"void main() {"
"  gl_FragColor =" V1 ";"
"}";

const char *fragment_shader_2_varying =
"varying vec4 v1;"
"varying vec4 v2;"
"void main() {"
"  gl_FragColor =" V1 "+" V2 ";"
"}";

const char *fragment_shader_4_varying =
"varying vec4 v1;"
"varying vec4 v2;"
"varying vec4 v3;"
"varying vec4 v4;"
"void main() {"
"  gl_FragColor =" V1 "+" V2 "+" V3 "+" V4 ";"
"}";

const char *fragment_shader_8_varying =
"varying vec4 v1;"
"varying vec4 v2;"
"varying vec4 v3;"
"varying vec4 v4;"
"varying vec4 v5;"
"varying vec4 v6;"
"varying vec4 v7;"
"varying vec4 v8;"
"void main() {"
"  gl_FragColor =" V1 "+" V2 "+" V3 "+" V4 "+" V5 "+" V6 "+" V7 "+" V8 ";"
"}";

GLuint VaryingsShaderProgram(int varyings_count, GLuint vertex_buffer) {
  const char *vertex_shader = NULL;
  const char *fragment_shader = NULL;
  switch (varyings_count) {
    case 1:
      vertex_shader = vertex_shader_1_varying;
      fragment_shader = fragment_shader_1_varying;
      break;
    case 2:
      vertex_shader = vertex_shader_2_varying;
      fragment_shader = fragment_shader_2_varying;
      break;
    case 4:
      vertex_shader = vertex_shader_4_varying;
      fragment_shader = fragment_shader_4_varying;
      break;
    case 8:
      vertex_shader = vertex_shader_8_varying;
      fragment_shader = fragment_shader_8_varying;
      break;
    default: return 0;
  }
  GLuint program =
    InitShaderProgram(vertex_shader, fragment_shader);

  int attribute_index = glGetAttribLocation(program, "c");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}


const char *fragment_shader_ddx =
"#extension GL_OES_standard_derivatives : enable\n"
"varying vec4 v1;"
"void main() {"
"  gl_FragColor = vec4(dFdx(" V1 ".x), 0., 0., 1.);"
"}";

const char *fragment_shader_ddy =
"#extension GL_OES_standard_derivatives : enable\n"
"varying vec4 v1;"
"void main() {"
"  gl_FragColor = vec4(dFdy(" V1 ".y), 0., 0., 1.);"
"}";

GLuint DdxDdyShaderProgram(bool ddx, GLuint vertex_buffer) {
  GLuint program =
    InitShaderProgram(vertex_shader_1_varying,
                      ddx ? fragment_shader_ddx : fragment_shader_ddy);

  int attribute_index = glGetAttribLocation(program, "c");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}


bool VaryingsAndDdxyShaderTest::Run() {
  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  const int c = 4;
  GLfloat *vertices = NULL;
  GLsizeiptr vertex_buffer_size = 0;
  CreateLattice(&vertices, &vertex_buffer_size, 1.f / c, 1.f / c, c, c);
  GLuint vertex_buffer = SetupVBO(GL_ARRAY_BUFFER,
                                  vertex_buffer_size, vertices);

  GLuint *indices = NULL;
  GLuint index_buffer = 0;
  GLsizeiptr index_buffer_size = 0;

  count_ = CreateMesh(&indices, &index_buffer_size, c, c, 0);
  index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                          index_buffer_size, indices);

  GLuint program = VaryingsShaderProgram(1, vertex_buffer);
  RunTest(this,
          "mpixels_sec_varyings_shader_1", g_width * g_height, true);
  glDeleteProgram(program);

  program = VaryingsShaderProgram(2, vertex_buffer);
  RunTest(this,
          "mpixels_sec_varyings_shader_2", g_width * g_height, true);
  glDeleteProgram(program);

  program = VaryingsShaderProgram(4, vertex_buffer);
  RunTest(this,
          "mpixels_sec_varyings_shader_4", g_width * g_height, true);
  glDeleteProgram(program);

  program = VaryingsShaderProgram(8, vertex_buffer);
  RunTest(this,
          "mpixels_sec_varyings_shader_8", g_width * g_height, true);
  glDeleteProgram(program);

#if !defined(DISABLE_SOME_TESTS_FOR_INTEL_DRIVER)
  program = DdxDdyShaderProgram(true, vertex_buffer);
  RunTest(this,
          "mpixels_sec_ddx_shader", g_width * g_height, true);
  glDeleteProgram(program);

  program = DdxDdyShaderProgram(false, vertex_buffer);
  RunTest(this,
          "mpixels_sec_ddy_shader", g_width * g_height, true);
  glDeleteProgram(program);
#endif

  glDeleteBuffers(1, &index_buffer);
  delete[] indices;

  glDeleteBuffers(1, &vertex_buffer);
  delete[] vertices;

  return true;
}


TestBase* GetVaryingsAndDdxyShaderTest() {
  return new VaryingsAndDdxyShaderTest;
}


} // namespace glbench
