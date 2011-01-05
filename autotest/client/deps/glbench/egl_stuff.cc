// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>

#include "base/logging.h"
#include "main.h"
#include "xlib_window.h"


EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLConfig egl_config = NULL;
static EGLSurface egl_surface = NULL;
static EGLContext egl_context = NULL;

#define CHECK_EGL() do { \
  if (eglGetError() != EGL_SUCCESS) return false; \
} while (0)

bool Init() {
  if (!XlibInit())
    return false;

  EGLNativeWindowType native_window =
      static_cast<EGLNativeWindowType>(g_xlib_window);
  egl_surface = eglCreateWindowSurface(g_egl_display, egl_config,
                                       native_window, NULL);
  CHECK_EGL();
  return true;
}

XVisualInfo* GetXVisual() {
  if (!egl_config) {
    EGLint attribs[] = {
      EGL_RED_SIZE, 1,
      EGL_GREEN_SIZE, 1,
      EGL_BLUE_SIZE, 1,
      EGL_DEPTH_SIZE, 1,
      EGL_STENCIL_SIZE, 1,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE
    };

    EGLNativeDisplayType native_display =
      static_cast<EGLNativeDisplayType>(g_xlib_display);

    g_egl_display = eglGetDisplay(native_display);
    CHECK_EGL();

    eglInitialize(g_egl_display, NULL, NULL);
    CHECK_EGL();

    EGLint num_configs = -1;
    eglGetConfigs(g_egl_display, NULL, 0, &num_configs);
    CHECK_EGL();

    eglChooseConfig(g_egl_display, attribs, &egl_config, 1, &num_configs);
    CHECK_EGL();
  }

  // TODO: for some reason on some systems EGL_NATIVE_VISUAL_ID returns an ID
  // that XVisualIDFromVisual cannot find.  Use default visual until this is
  // resolved.
#if 0
  EGLint visual_id;
  eglGetConfigAttrib(g_egl_display, egl_config, EGL_NATIVE_VISUAL_ID,
                     &visual_id);
  CHECK_EGL();
  XVisualInfo vinfo_template;
  vinfo_template.visualid = static_cast<VisualID>(visual_id);
#else
  XVisualInfo vinfo_template;
  vinfo_template.visualid = XVisualIDFromVisual(DefaultVisual(
      g_xlib_display, DefaultScreen(g_xlib_display)));
#endif

  int nitems = 0;
  XVisualInfo* ret = XGetVisualInfo(g_xlib_display, VisualIDMask,
                                    &vinfo_template, &nitems);
  CHECK(nitems == 1);
  return ret;
}

bool InitContext() {
  EGLint attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  EGLContext egl_context = eglCreateContext(g_egl_display, egl_config,
                                            NULL, attribs);
  CHECK_EGL();

  eglMakeCurrent(g_egl_display, egl_surface, egl_surface, egl_context);
  CHECK_EGL();

  eglQuerySurface(g_egl_display, egl_surface, EGL_WIDTH, &g_width);
  eglQuerySurface(g_egl_display, egl_surface, EGL_HEIGHT, &g_height);

  return true;
}

void DestroyContext() {
  eglMakeCurrent(g_egl_display, NULL, NULL, NULL);
  eglDestroyContext(g_egl_display, egl_context);
}

void TerminateGL() {
  eglDestroySurface(g_egl_display, egl_surface);
  eglTerminate(g_egl_display);
}

void SwapBuffers() {
  eglSwapBuffers(g_egl_display, egl_surface);
}

bool SwapInterval(int interval) {
  return eglSwapInterval(g_egl_display, interval) == EGL_TRUE;
}
