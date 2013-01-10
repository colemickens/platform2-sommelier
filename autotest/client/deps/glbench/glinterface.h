// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_GLINTERFACE_H_
#define BENCH_GL_GLINTERFACE_H_

#include <X11/Xutil.h>

#include "base/memory/scoped_ptr.h"

class GLInterface {
 public:
  GLInterface() {}
  virtual ~GLInterface() {}
  virtual bool Init() = 0;
  virtual XVisualInfo* GetXVisual() = 0;

  virtual bool InitContext() = 0;
  virtual void DestroyContext() = 0;
  virtual void SwapBuffers() = 0;
  virtual bool SwapInterval(int interval) = 0;

  virtual void CheckError() = 0;

  static GLInterface* Create();
};

extern scoped_ptr<GLInterface> g_main_gl_interface;

#endif  // BENCH_GL_GLINTERFACE_H_
