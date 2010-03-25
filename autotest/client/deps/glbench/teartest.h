// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_TEARTEST_H_
#define BENCH_GL_TEARTEST_H_

#include <X11/Xlib.h>

enum TestState {
  TestStart,
  TestLoop,
  TestStop
};

typedef bool (*Test)(TestState state, int arg);

void InitializePixmap();
void UpdatePixmap(int i);
void CopyPixmapToTexture();

// TODO: implement EGL counterpart.
void InitNative(Pixmap pixmap);
bool UpdateBindTexImage(TestState state, int arg);

#endif // BENCH_GL_TEARTEST_H_
