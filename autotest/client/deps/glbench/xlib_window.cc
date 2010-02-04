// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "main.h"
#include "xlib_window.h"

Display *xlib_display = NULL;
Window xlib_window = 0;

GLint g_width = 0;
GLint g_height = 0;

bool XlibInit() {
  xlib_display = ::XOpenDisplay(0);
  if (!xlib_display)
    return false;

  int screen = DefaultScreen(xlib_display);
  Window root_window = RootWindow(xlib_display, screen);

  int attrib[] = {
    GLX_RGBA,
    GLX_DOUBLEBUFFER, 1,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_DEPTH_SIZE, 1,
    GLX_STENCIL_SIZE, 1,
    None
  };
  XVisualInfo *visinfo = glXChooseVisual(xlib_display, screen, attrib);
  unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask |
    CWOverrideRedirect;
  XSetWindowAttributes attr;
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  attr.colormap = XCreateColormap(xlib_display, root_window, visinfo->visual,
                                  AllocNone);
  attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
  attr.override_redirect = True;
  xlib_window = ::XCreateWindow(xlib_display, root_window,
                                0, 0, 512, 512, 0, visinfo->depth, InputOutput,
                                visinfo->visual, mask, &attr);

  ::XMapWindow(xlib_display, xlib_window);
  ::XSync(xlib_display, True);

  XWindowAttributes attributes;
  ::XGetWindowAttributes(xlib_display, xlib_window, &attributes);
  g_width = attributes.width;
  g_height = attributes.height;

  return true;
}


