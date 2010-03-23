// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "main.h"
#include "xlib_window.h"
#include "base/logging.h"


Display *xlib_display = NULL;
Window xlib_window = 0;
XVisualInfo *xlib_visinfo = NULL;

GLint g_width = 512;
GLint g_height = 512;
bool g_override_redirect = true;


bool XlibInit() {
  xlib_display = XOpenDisplay(0);
  if (!xlib_display)
    return false;

  int screen = DefaultScreen(xlib_display);
  Window root_window = RootWindow(xlib_display, screen);

  XWindowAttributes attributes;
  XGetWindowAttributes(xlib_display, root_window, &attributes);

  g_width = g_width == -1 ? attributes.width : g_width;
  g_height = g_height == -1 ? attributes.height : g_height;

  XVisualInfo vinfo_template;
  memset(&vinfo_template, sizeof(vinfo_template), 0);
  vinfo_template.visualid = GetVisualID();
  int nitems;
  xlib_visinfo = XGetVisualInfo(xlib_display,
                                VisualIDMask, &vinfo_template, &nitems);
  CHECK(nitems == 1);

  unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask |
    CWOverrideRedirect;
  XSetWindowAttributes attr;
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  attr.colormap = XCreateColormap(xlib_display, root_window,
                                  xlib_visinfo->visual, AllocNone);
  attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
  attr.override_redirect = g_override_redirect ? True : False;
  xlib_window = XCreateWindow(xlib_display, root_window,
                              0, 0, g_width, g_height, 0, xlib_visinfo->depth,
                              InputOutput, xlib_visinfo->visual, mask, &attr);

  XMapWindow(xlib_display, xlib_window);
  XSync(xlib_display, True);

  XGetWindowAttributes(xlib_display, xlib_window, &attributes);
  g_width = attributes.width;
  g_height = attributes.height;

  return true;
}
