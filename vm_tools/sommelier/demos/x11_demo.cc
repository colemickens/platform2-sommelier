// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "base/logging.h"

// Creates an X window the same size as the display and fills its background
// with a solid color that can be specified as the only parameter (in hex or
// base 10). Closes on any keypress.
int main(int argc, char* argv[]) {
  uint32_t bgcolor = 0xFF0000;
  if (argc > 1) {
    bgcolor = strtoul(argv[1], nullptr, 0);
  }

  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    LOG(ERROR) << "Failed opening display";
    return -1;
  }

  int screen = DefaultScreen(dpy);
  Window win;
  int x, y;
  unsigned int width, height, border, depth;
  if (XGetGeometry(dpy, RootWindow(dpy, screen), &win, &x, &y, &width, &height,
                   &border, &depth) == 0) {
    LOG(ERROR) << "Failed getting screen geometry";
    return -1;
  }
  win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), x, y, width, height,
                            0, 0 /* black */, bgcolor);

  XClassHint* wmclass_hint = XAllocClassHint();
  char class_name[] = "x11_demo";
  wmclass_hint->res_name = wmclass_hint->res_class = class_name;
  XSetClassHint(dpy, win, wmclass_hint);
  XSelectInput(dpy, win, KeyPressMask);
  XMapWindow(dpy, win);

  XEvent evt;
  for (;;) {
    XNextEvent(dpy, &evt);
    if (evt.type == KeyPress)
      break;
  }

  XCloseDisplay(dpy);
  return 0;
}
