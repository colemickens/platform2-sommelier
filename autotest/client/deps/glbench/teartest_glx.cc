// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/logging.h"

#include "main.h"
#include "xlib_window.h"
#include "glx_stuff.h"
#include "teartest.h"


PFNGLXBINDTEXIMAGEEXTPROC glXBindTexImageEXT = NULL;
PFNGLXRELEASETEXIMAGEEXTPROC glXReleaseTexImageEXT = NULL;

static GLXPixmap glxpixmap = 0;


void InitNative(Pixmap pixmap) {
  glXBindTexImageEXT = reinterpret_cast<PFNGLXBINDTEXIMAGEEXTPROC>(
      glXGetProcAddress(
          reinterpret_cast<const GLubyte *>("glXBindTexImageEXT")));
  glXReleaseTexImageEXT = reinterpret_cast<PFNGLXRELEASETEXIMAGEEXTPROC>(
      glXGetProcAddress(
          reinterpret_cast<const GLubyte *>("glXReleaseTexImageEXT")));

  if (!glXBindTexImageEXT)
    return;

  int rgba, rgb;
  glXGetFBConfigAttrib(g_xlib_display, g_glx_fbconfig,
                       GLX_BIND_TO_TEXTURE_RGBA_EXT, &rgba);
  glXGetFBConfigAttrib(g_xlib_display, g_glx_fbconfig,
                       GLX_BIND_TO_TEXTURE_RGB_EXT, &rgb);
  CHECK(rgba || rgb);
  const int pixmapAttribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT,
        rgba ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT,
    None
  };

  glxpixmap = glXCreatePixmap(g_xlib_display, g_glx_fbconfig,
                              pixmap, pixmapAttribs);
}

bool UpdateBindTexImage(TestState state, int shift) {
  if (!glXBindTexImageEXT) {
    printf("# Could not load glXBindTexImageEXT\n");
    return false;
  }

  switch (state) {
    case TestStart:
      printf("# Update pixmap bound to texture.\n");
      InitializePixmap();
      CopyPixmapToTexture();
      glXBindTexImageEXT(g_xlib_display, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
      break;

    case TestLoop:
      glXReleaseTexImageEXT(g_xlib_display, glxpixmap, GLX_FRONT_LEFT_EXT);
      UpdatePixmap(shift);
      glXBindTexImageEXT(g_xlib_display, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
      break;

    case TestStop:
      glXReleaseTexImageEXT(g_xlib_display, glxpixmap, GLX_FRONT_LEFT_EXT);
      break;
  }
  return true;
}
