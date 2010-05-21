// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_UTILS_H_
#define BENCH_GL_UTILS_H_

#ifdef USE_EGL
#include "GLES2/gl2.h"
#else
#include "GL/gl.h"
#endif


void SetBasePathFromArgv0(const char* argv0, const char* relative);
void *MmapFile(const char *name, size_t *length);

namespace glbench {

GLuint SetupTexture(GLsizei size_log2);
GLuint SetupVBO(GLenum target, GLsizeiptr size, const GLvoid *data);
void CreateLattice(GLfloat **vertices, GLsizeiptr *size,
                   GLfloat size_x, GLfloat size_y, int width, int height);
int CreateMesh(GLuint **indices, GLsizeiptr *size,
                      int width, int height, int culled_ratio);
GLuint InitShaderProgram(const char *vertex_src, const char *fragment_src);

#define I915_WORKAROUND 1

} // namespace glbench

#endif // BENCH_GL_UTILS_H_
