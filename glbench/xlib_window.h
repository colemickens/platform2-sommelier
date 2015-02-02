// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GLBENCH_XLIB_WINDOW_H_
#define GLBENCH_XLIB_WINDOW_H_

#include <X11/Xlib.h>

extern Display *g_xlib_display;
extern Window g_xlib_window;

bool XlibInit();

#endif  // GLBENCH_XLIB_WINDOW_H_
