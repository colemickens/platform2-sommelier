// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_MAIN_H_
#define BENCH_GL_MAIN_H_

#include <sys/time.h>

#ifdef USE_GLES
#include <EGL/egl.h>
#include <GLES/gl.h>
#else
#include <GL/gl.h>

#define LIST_PROC_FUNCTIONS(F) \
    F(glAttachShader, PFNGLATTACHSHADERPROC) \
    F(glBindBuffer, PFNGLBINDBUFFERPROC) \
    F(glBufferData, PFNGLBUFFERDATAPROC) \
    F(glCompileShader, PFNGLCOMPILESHADERPROC) \
    F(glCreateProgram, PFNGLCREATEPROGRAMPROC) \
    F(glCreateShader, PFNGLCREATESHADERPROC) \
    F(glDeleteBuffers, PFNGLDELETEBUFFERSPROC) \
    F(glDeleteProgram, PFNGLDELETEPROGRAMPROC) \
    F(glDeleteShader, PFNGLDELETESHADERPROC) \
    F(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC) \
    F(glGenBuffers, PFNGLGENBUFFERSPROC) \
    F(glGetAttribLocation, PFNGLGETATTRIBLOCATIONPROC) \
    F(glGetInfoLogARB, PFNGLGETPROGRAMINFOLOGPROC) \
    F(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC) \
    F(glLinkProgram, PFNGLLINKPROGRAMPROC) \
    F(glShaderSource, PFNGLSHADERSOURCEPROC) \
    F(glUniform1f, PFNGLUNIFORM1FPROC) \
    F(glUniform1i, PFNGLUNIFORM1IPROC) \
    F(glUseProgram, PFNGLUSEPROGRAMPROC) \
    F(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC)

#define F(fun, type) extern type fun;
LIST_PROC_FUNCTIONS(F)
#undef F

#endif

inline uint64_t GetUTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<uint64_t>(tv.tv_usec) +
    1000000ULL*static_cast<uint64_t>(tv.tv_sec);
}

extern GLint g_width;
extern GLint g_height;

bool Init();
bool InitContext();
void DestroyContext();
void SwapBuffers();

// BenchFunc functions are assumed to execute an operation iter times.  See
// Bench for a detailed explanation.
typedef void (*BenchFunc)(int iter);

uint64_t TimeBench(BenchFunc func, int iter);

#define MAX_ITERATION_DURATION_MS 100000
// Runs func passing it sequential powers of two (8, 16, 32,...) recording time
// it took.  The data is then fitted linearly, obtaining slope and bias such
// that:
//   time it took to run x iterations = slope * x + bias
// Returns false if one iteration of the test takes longer than
// MAX_ITERATION_LENGTH_MS.  The test is then assumed too slow to provide
// meaningful results.
bool Bench(BenchFunc func, float *slope, int64_t *bias);

void *MmapFile(const char *name, size_t *length);

#endif  // BENCH_GL_MAIN_H_
