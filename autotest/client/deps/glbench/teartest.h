// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_TEARTEST_H_
#define BENCH_GL_TEARTEST_H_

#include <X11/Xlib.h>


class Test {
 public:
  virtual bool Start() = 0;
  virtual bool Loop(int shift) = 0;
  virtual void Stop() = 0;
  virtual ~Test() {}
};


Pixmap AllocatePixmap();
void InitializePixmap(Pixmap pixmap);
void UpdatePixmap(Pixmap pixmap, int i);
void CopyPixmapToTexture(Pixmap pixmap);

Test* GetUniformTest();
Test* GetTexImage2DTest();
#ifdef USE_EGL
Test* GetPixmapToTextureTestEGL();
#else
Test* GetPixmapToTextureTest();
#endif


#endif // BENCH_GL_TEARTEST_H_
