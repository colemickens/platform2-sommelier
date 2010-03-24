// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>

#include "base/logging.h"
#include "main.h"
#include "xlib_window.h"


static EGLDisplay egl_display = NULL;
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
  egl_surface = eglCreateWindowSurface(egl_display, egl_config,
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
      EGL_NONE
    };

    EGLNativeDisplayType native_display =
      static_cast<EGLNativeDisplayType>(g_xlib_display);

    egl_display = eglGetDisplay(native_display);
    CHECK_EGL();

    eglInitialize(egl_display, NULL, NULL);
    CHECK_EGL();

    EGLint num_configs = -1;
    eglGetConfigs(egl_display, NULL, 0, &num_configs);
    CHECK_EGL();

    eglChooseConfig(egl_display, attribs, &egl_config, 1, &num_configs);
    CHECK_EGL();
  }

  EGLint visual_id;
  eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &visual_id);
  CHECK_EGL();

  XVisualInfo vinfo_template;
  vinfo_template.visualid = static_cast<VisualID>(visual_id);
  int nitems = 0;
  XVisualInfo* ret = XGetVisualInfo(g_xlib_display, VisualIDMask,
                                    &vinfo_template, &nitems);
  CHECK(nitems == 1);
  return ret;
}

bool InitContext() {
  EGLContext egl_context = eglCreateContext(egl_display, egl_config,
                                            NULL, NULL);
  CHECK_EGL();

  eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
  CHECK_EGL();

  eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &g_width);
  eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &g_height);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  return true;
}

void DestroyContext() {
  eglMakeCurrent(egl_display, NULL, NULL, NULL);
  eglDestroyContext(egl_display, egl_context);
}

void TerminateGL() {
  eglDestroySurface(egl_display, egl_surface);
  eglTerminate(egl_display);
}

void SwapBuffers() {
  eglSwapBuffers(egl_display, egl_surface);
}

bool SwapInterval(int interval) {
  return eglSwapInterval(egl_display, interval) == EGL_TRUE;
}
