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
    F(glGenBuffers, PFNGLGENBUFFERSPROC) \
    F(glBindBuffer, PFNGLBINDBUFFERPROC) \
    F(glBufferData, PFNGLBUFFERDATAPROC) \
    F(glDeleteBuffers, PFNGLDELETEBUFFERSPROC)

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

typedef void (*BenchFunc)(int iter);

uint64_t TimeBench(BenchFunc func, int iter);
void Bench(BenchFunc func, float *slope, int64_t *bias);

#endif  // BENCH_GL_MAIN_H_
