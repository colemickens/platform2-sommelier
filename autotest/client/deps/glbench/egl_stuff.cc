// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "egl_stuff.h"
#include "main.h"
#include "xlib_window.h"

#define CHECK_EGL() CHECK_EQ(eglGetError(), EGL_SUCCESS)

scoped_ptr<GLInterface> g_main_gl_interface;

GLInterface* GLInterface::Create() {
  return new EGLInterface;
}

bool EGLInterface::Init() {
  if (!XlibInit())
    return false;

  EGLNativeWindowType native_window =
      static_cast<EGLNativeWindowType>(g_xlib_window);
  surface_ = eglCreateWindowSurface(display_, config_, native_window, NULL);
  CHECK_EGL();
  return true;
}

XVisualInfo* EGLInterface::GetXVisual() {
  if (!config_) {
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

    display_ = eglGetDisplay(native_display);
    CHECK_EGL();

    eglInitialize(display_, NULL, NULL);
    CHECK_EGL();

    EGLint num_configs = -1;
    eglGetConfigs(display_, NULL, 0, &num_configs);
    CHECK_EGL();

    eglChooseConfig(display_, attribs, &config_, 1, &num_configs);
    CHECK_EGL();
  }

  // TODO: for some reason on some systems EGL_NATIVE_VISUAL_ID returns an ID
  // that XVisualIDFromVisual cannot find.  Use default visual until this is
  // resolved.
#if 0
  EGLint visual_id;
  eglGetConfigAttrib(display_, config_, EGL_NATIVE_VISUAL_ID, &visual_id);
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

bool EGLInterface::InitContext() {
  EGLint attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  context_ = eglCreateContext(display_, config_, NULL, attribs);
  CHECK_EGL();

  eglMakeCurrent(display_, surface_, surface_, context_);
  CHECK_EGL();

  eglQuerySurface(display_, surface_, EGL_WIDTH, &g_width);
  eglQuerySurface(display_, surface_, EGL_HEIGHT, &g_height);

  return true;
}

void EGLInterface::DestroyContext() {
  eglMakeCurrent(display_, NULL, NULL, NULL);
  eglDestroyContext(display_, context_);
}

void EGLInterface::SwapBuffers() {
  eglSwapBuffers(display_, surface_);
}

bool EGLInterface::SwapInterval(int interval) {
  return (eglSwapInterval(display_, interval) == EGL_TRUE);
}

void EGLInterface::TerminateGL() {
  eglDestroySurface(display_, surface_);
  eglTerminate(display_);
}
