// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_GLINTERFACE_H_
#define BENCH_GL_GLINTERFACE_H_

#include <X11/Xutil.h>

#include "base/memory/scoped_ptr.h"

#if defined(USE_OPENGL)

struct __GLXcontextRec;  // Forward declaration from GLX.h.
typedef struct __GLXcontextRec *GLXContext;
typedef GLXContext GLContext;

#elif defined(USE_OPENGLES)

typedef void *EGLContext;  // Forward declaration from EGL.h.
typedef EGLContext GLContext;

#endif

class GLInterface {
 public:
  GLInterface() {}
  virtual ~GLInterface() {}
  virtual bool Init() = 0;
  virtual void Cleanup() = 0;
  virtual XVisualInfo* GetXVisual() = 0;

  virtual void SwapBuffers() = 0;
  virtual bool SwapInterval(int interval) = 0;

  virtual void CheckError() = 0;

  virtual bool MakeCurrent(const GLContext& context) = 0;
  virtual const GLContext CreateContext() = 0;
  virtual void DeleteContext(const GLContext& context) = 0;
  virtual const GLContext& GetMainContext() = 0;

  static GLInterface* Create();
};

extern scoped_ptr<GLInterface> g_main_gl_interface;

#endif  // BENCH_GL_GLINTERFACE_H_
