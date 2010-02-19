// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <assert.h>

#include "main.h"
#include "shaders.h"


static void print_info_log(int obj)
{
  char info_log[4096];
  int length;
  glGetInfoLogARB(obj, sizeof(info_log)-1, &length, info_log);
  if (length)
    printf("Log: %s\n", info_log);
}

static ShaderProgram InitShaderProgram(const char *vertex_src,
                                       const char *fragment_src) {
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_src, NULL);
  glShaderSource(fragment_shader, 1, &fragment_src, NULL);

  glCompileShader(vertex_shader);
  print_info_log(vertex_shader);
  glCompileShader(fragment_shader);
  print_info_log(fragment_shader);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  print_info_log(program);
  glUseProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

void DeleteShaderProgram(ShaderProgram program) {
  if (!program)
    return;

  glUseProgram(0);
  glDeleteProgram(program);
}


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
  const char *vertex_shader;
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
  assert(glGetError() == 0);

  return program;
}
