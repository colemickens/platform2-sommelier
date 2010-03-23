// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <GL/glx.h>

#include "base/logging.h"
#include "main.h"
#include "xlib_window.h"

static GLXContext glx_context = NULL;

#define F(fun, type) type fun = NULL;
LIST_PROC_FUNCTIONS(F)
#undef F

bool Init() {
  return XlibInit();
}

VisualID GetVisualID() {
  int screen = DefaultScreen(g_xlib_display);
  int attrib[] = {
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_DEPTH_SIZE, 1,
    GLX_STENCIL_SIZE, 1,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    None
  };
  int nelements;
  GLXFBConfig *fbconfigs = glXChooseFBConfig(g_xlib_display, screen,
                                             attrib, &nelements);
  CHECK(nelements >= 1);
  int visual_id;
  glXGetFBConfigAttrib(g_xlib_display, fbconfigs[0], GLX_VISUAL_ID, &visual_id);
  XFree(fbconfigs);
  return static_cast<VisualID>(visual_id);
}

bool InitContext() {
  glx_context = glXCreateContext(g_xlib_display, g_xlib_visinfo, 0, True);
  if (!glx_context)
    return false;

  if (!glXMakeCurrent(g_xlib_display, g_xlib_window, glx_context)) {
    glXDestroyContext(g_xlib_display, glx_context);
    return false;
  }

  glClearColor(1.f, 0, 0, 1.f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  SwapBuffers();
  glClearColor(0, 1.f, 0, 1.f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  SwapBuffers();
  glClearColor(0, 0, 0.f, 0.f);

  const GLubyte *str = glGetString(GL_EXTENSIONS);
  if (!str || !strstr(reinterpret_cast<const char *>(str),
                      "GL_ARB_vertex_buffer_object"))
    return false;

#define F(fun, type) fun = reinterpret_cast<type>( \
    glXGetProcAddress(reinterpret_cast<const GLubyte *>(#fun)));
  LIST_PROC_FUNCTIONS(F)
#undef F

  return true;
}

void DestroyContext() {
  glXMakeCurrent(g_xlib_display, 0, NULL);
  glXDestroyContext(g_xlib_display, glx_context);
}

void SwapBuffers() {
  glXSwapBuffers(g_xlib_display, g_xlib_window);
}

bool SwapInterval(int interval) {
  return glXSwapIntervalSGI(interval) == 0;
}
