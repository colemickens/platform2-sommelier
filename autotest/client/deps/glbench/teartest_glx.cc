// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/logging.h"

#include "main.h"
#include "xlib_window.h"
#include "glx_stuff.h"
#include "teartest.h"


class PixmapToTextureTest : public Test {
 public:
  PixmapToTextureTest();
  virtual bool Start();
  virtual bool Loop(int shift);
  virtual void Stop();
 private:
  bool InitNative();

  GLXPixmap glxpixmap_;
  Pixmap pixmap_;
  bool init_succeeded;
};

Test* GetPixmapToTextureTest() {
  return new PixmapToTextureTest();
}


PixmapToTextureTest::PixmapToTextureTest() :
  glxpixmap_(0),
  pixmap_(0),
  init_succeeded(false) {}


bool PixmapToTextureTest::InitNative() {
  int major = 0;
  int minor = 0;
  if (!glXQueryVersion(g_xlib_display, &major, &minor))
    return false;

  if (major < 1 || (major == 1 && minor < 3))
    return false;

  if (!glXBindTexImageEXT)
    return false;

  pixmap_ = AllocatePixmap();

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

  glxpixmap_ = glXCreatePixmap(g_xlib_display, g_glx_fbconfig,
                              pixmap_, pixmapAttribs);
  return true;
}


bool PixmapToTextureTest::Start() {
  init_succeeded = InitNative();
  printf("# Update pixmap bound to texture.\n");
  CopyPixmapToTexture(pixmap_);
  glXBindTexImageEXT(g_xlib_display, glxpixmap_, GLX_FRONT_LEFT_EXT, NULL);
  return true;
}


bool PixmapToTextureTest::Loop(int shift) {
  if (!init_succeeded)
    return false;
  glXReleaseTexImageEXT(g_xlib_display, glxpixmap_, GLX_FRONT_LEFT_EXT);
  UpdatePixmap(pixmap_, shift);
  glXBindTexImageEXT(g_xlib_display, glxpixmap_, GLX_FRONT_LEFT_EXT, NULL);
  return true;
}


void PixmapToTextureTest::Stop() {
  glXReleaseTexImageEXT(g_xlib_display, glxpixmap_, GLX_FRONT_LEFT_EXT);
  glFinish();
  glXDestroyPixmap(g_xlib_display, glxpixmap_);
  XFreePixmap(g_xlib_display, pixmap_);
}
