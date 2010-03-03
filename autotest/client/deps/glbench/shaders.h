// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_SHADERS_H_
#define BENCH_GL_SHADERS_H_

typedef GLuint ShaderProgram;

void DeleteShaderProgram(ShaderProgram program);
ShaderProgram AttributeFetchShaderProgram(int attribute_count,
                                          GLuint vertex_buffers[]);
ShaderProgram VaryingsShaderProgram(int varyings_count, GLuint vertex_buffer);
ShaderProgram DdxDdyShaderProgram(bool ddx, GLuint vertex_buffer);
ShaderProgram YuvToRgbShaderProgram(GLuint vertex_buffer,
                                    int width, int height);

#endif // BENCH_GL_SHADERS_H_
