// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <sys/mman.h>

#include "base/logging.h"

#include "main.h"
#include "shaders.h"
#include "utils.h"
#include "yuv2rgb.h"


const char *simple_vertex_shader =
"attribute vec4 c1;"
"void main() {"
"    gl_Position = c1;"
"}";

const char *simple_vertex_shader_2_attr =
"attribute vec4 c1;"
"attribute vec4 c2;"
"void main() {"
"    gl_Position = c1+c2;"
"}";

const char *simple_vertex_shader_4_attr =
"attribute vec4 c1;"
"attribute vec4 c2;"
"attribute vec4 c3;"
"attribute vec4 c4;"
"void main() {"
"    gl_Position = c1+c2+c3+c4;"
"}";

const char *simple_vertex_shader_8_attr =
"attribute vec4 c1;"
"attribute vec4 c2;"
"attribute vec4 c3;"
"attribute vec4 c4;"
"attribute vec4 c5;"
"attribute vec4 c6;"
"attribute vec4 c7;"
"attribute vec4 c8;"
"void main() {"
"    gl_Position = c1+c2+c3+c4+c5+c6+c7+c8;"
"}";


const char *simple_fragment_shader =
"void main() {"
"    gl_FragColor = vec4(0.5);"
"}";


ShaderProgram AttributeFetchShaderProgram(int attribute_count,
                                          GLuint vertex_buffers[]) {
  const char *vertex_shader = NULL;
  switch (attribute_count) {
    case 1: vertex_shader = simple_vertex_shader; break;
    case 2: vertex_shader = simple_vertex_shader_2_attr; break;
    case 4: vertex_shader = simple_vertex_shader_4_attr; break;
    case 8: vertex_shader = simple_vertex_shader_8_attr; break;
    default: return 0;
  }
  ShaderProgram program =
    InitShaderProgram(vertex_shader, simple_fragment_shader);

  for (int i = 0; i < attribute_count; i++) {
    char attribute[] = "c_";
    attribute[1] = '1' + i;
    int attribute_index = glGetAttribLocation(program, attribute);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[i]);
    glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(attribute_index);
  }

  return program;
}

#define I915_WORKAROUND 1

#if I915_WORKAROUND
#define V1 "gl_TexCoord[0]"
#define V2 "gl_TexCoord[1]"
#define V3 "gl_TexCoord[2]"
#define V4 "gl_TexCoord[3]"
#define V5 "gl_TexCoord[4]"
#define V6 "gl_TexCoord[5]"
#define V7 "gl_TexCoord[6]"
#define V8 "gl_TexCoord[7]"
#define DDX "dFdx"
#define DDY "dFdy"
#else
#define V1 "v1"
#define V2 "v2"
#define V3 "v3"
#define V4 "v4"
#define V5 "v5"
#define V6 "v6"
#define V7 "v7"
#define V8 "v8"
#define DDX "ddx"
#define DDY "ddy"
#endif

const char *basic_texture_vertex_shader =
"attribute vec4 c1;"
"attribute vec4 c2;"
"varying vec2 v1;"
"void main() {"
"    gl_Position = c1;"
"    " V1 " = c2;"
"}";

const char *basic_texture_fragment_shader =
"uniform sampler2D texture_sampler;"
"varying vec2 v1;"
"void main() {"
"    gl_FragColor = texture2D(texture_sampler, " V1 ".xy);"
"}";

ShaderProgram BasicTextureShaderProgram(GLuint vertex_buffer,
                                        GLuint texture_buffer) {
  ShaderProgram program =
    InitShaderProgram(basic_texture_vertex_shader,
                      basic_texture_fragment_shader);

  // Set up the texture sampler
  int textureSampler = glGetUniformLocation(program, "texture_sampler");
  glUniform1i(textureSampler, 0);

  // Set up vertex attribute
  int attribute_index = glGetAttribLocation(program, "c1");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  // Set up texture attribute
  attribute_index = glGetAttribLocation(program, "c2");
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}

const char *double_texture_blend_vertex_shader =
"attribute vec4 c1;"
"attribute vec4 c2;"
"attribute vec4 c3;"
"varying vec2 v1;"
"varying vec2 v2;"
"void main() {"
"    gl_Position = c1;"
"    " V1 " = c2;"
"    " V2 " = c3;"
"}";

const char *double_texture_blend_fragment_shader =
"uniform sampler2D texture_sampler_0;"
"uniform sampler2D texture_sampler_1;"
"varying vec2 v1;"
"varying vec2 v2;"
"void main() {"
"    vec4 one = texture2D(texture_sampler_0, " V1 ".xy);"
"    vec4 two = texture2D(texture_sampler_1, " V2 ".xy);"
"    gl_FragColor = mix(one, two, 0.5);"
"}";

// This shader blends the three textures
ShaderProgram DoubleTextureBlendShaderProgram(GLuint vertex_buffer,
                                              GLuint texture_buffer_0,
                                              GLuint texture_buffer_1) {
  ShaderProgram program =
    InitShaderProgram(double_texture_blend_vertex_shader,
                      double_texture_blend_fragment_shader);

  // Set up the texture sampler
  int textureSampler0 = glGetUniformLocation(program, "texture_sampler_0");
  glUniform1i(textureSampler0, 0);
  int textureSampler1 = glGetUniformLocation(program, "texture_sampler_1");
  glUniform1i(textureSampler1, 1);

  // Set up vertex attribute
  int attribute_index = glGetAttribLocation(program, "c1");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  // Set up texture attributes
  attribute_index = glGetAttribLocation(program, "c2");
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer_0);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  attribute_index = glGetAttribLocation(program, "c3");
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer_1);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}

const char *triple_texture_blend_vertex_shader =
"attribute vec4 c1;"
"attribute vec4 c2;"
"attribute vec4 c3;"
"attribute vec4 c4;"
"varying vec2 v1;"
"varying vec2 v2;"
"varying vec2 v3;"
"void main() {"
"    gl_Position = c1;"
"    " V1 " = c2;"
"    " V2 " = c3;"
"    " V3 " = c4;"
"}";

const char *triple_texture_blend_fragment_shader =
"uniform sampler2D texture_sampler_0;"
"uniform sampler2D texture_sampler_1;"
"uniform sampler2D texture_sampler_2;"
"varying vec2 v1;"
"varying vec2 v2;"
"varying vec2 v3;"
"void main() {"
"    vec4 one = texture2D(texture_sampler_0, " V1 ".xy);"
"    vec4 two = texture2D(texture_sampler_1, " V2 ".xy);"
"    vec4 three = texture2D(texture_sampler_2, " V3 ".xy);"
"    gl_FragColor = mix(mix(one, two, 0.5), three, 0.5);"
"}";

// This shader blends the three textures
ShaderProgram TripleTextureBlendShaderProgram(GLuint vertex_buffer,
                                              GLuint texture_buffer_0,
                                              GLuint texture_buffer_1,
                                              GLuint texture_buffer_2) {
  ShaderProgram program =
    InitShaderProgram(triple_texture_blend_vertex_shader,
                      triple_texture_blend_fragment_shader);

  // Set up the texture sampler
  int textureSampler0 = glGetUniformLocation(program, "texture_sampler_0");
  glUniform1i(textureSampler0, 0);
  int textureSampler1 = glGetUniformLocation(program, "texture_sampler_1");
  glUniform1i(textureSampler1, 1);
  int textureSampler2 = glGetUniformLocation(program, "texture_sampler_2");
  glUniform1i(textureSampler2, 2);

  // Set up vertex attribute
  int attribute_index = glGetAttribLocation(program, "c1");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  // Set up texture attributes
  attribute_index = glGetAttribLocation(program, "c2");
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer_0);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  attribute_index = glGetAttribLocation(program, "c3");
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer_1);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  attribute_index = glGetAttribLocation(program, "c4");
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer_2);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}

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

const char *fragment_shader_ddx =
"varying vec4 v1;"
"void main() {"
"  gl_FragColor = vec4(" DDX "(" V1 ".x), 0., 0., 1.);"
"}";

const char *fragment_shader_ddy =
"varying vec4 v1;"
"void main() {"
"  gl_FragColor = vec4(" DDY "(" V1 ".y), 0., 0., 1.);"
"}";

#undef V1
#undef V2
#undef V3
#undef V4
#undef V5
#undef V6
#undef V7
#undef V8
#undef DDX
#undef DDY


ShaderProgram VaryingsShaderProgram(int varyings_count, GLuint vertex_buffer) {
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
  ShaderProgram program =
    InitShaderProgram(vertex_shader, fragment_shader);

  int attribute_index = glGetAttribLocation(program, "c");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}


ShaderProgram DdxDdyShaderProgram(bool ddx, GLuint vertex_buffer) {
  ShaderProgram program =
    InitShaderProgram(vertex_shader_1_varying,
                      ddx ? fragment_shader_ddx : fragment_shader_ddy);

  int attribute_index = glGetAttribLocation(program, "c");
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(attribute_index);

  return program;
}

ShaderProgram YuvToRgbShaderProgram(int type, GLuint vertex_buffer,
                                    int width, int height) {
  const char *vertex = type == 1 ? YUV2RGB_VERTEX_1 : YUV2RGB_VERTEX_2; 
  const char *fragment = type == 1 ? YUV2RGB_FRAGMENT_1 : YUV2RGB_FRAGMENT_2;
  size_t size_vertex = 0;
  size_t size_fragment = 0;
  char *yuv_to_rgb_vertex = static_cast<char *>(
      MmapFile(vertex, &size_vertex));
  char *yuv_to_rgb_fragment = static_cast<char *>(
      MmapFile(fragment, &size_fragment));
  ShaderProgram program = 0;

  if (!yuv_to_rgb_fragment || !yuv_to_rgb_vertex)
    goto done;

  {
    program = InitShaderProgram(yuv_to_rgb_vertex,
                                yuv_to_rgb_fragment);

    int imageWidthUniform = glGetUniformLocation(program, "imageWidth");
    int imageHeightUniform = glGetUniformLocation(program, "imageHeight");
    int textureSampler = glGetUniformLocation(program, "textureSampler");
    int evenLinesSampler = glGetUniformLocation(program, "paritySampler");

    glUniform1f(imageWidthUniform, width);
    glUniform1f(imageHeightUniform, height);
    glUniform1i(textureSampler, 0);
    glUniform1i(evenLinesSampler, 1);

    int attribute_index = glGetAttribLocation(program, "c");
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(attribute_index);
    return program;
  }


done:
  munmap(yuv_to_rgb_fragment, size_fragment);
  munmap(yuv_to_rgb_fragment, size_vertex);
  return program;
}
