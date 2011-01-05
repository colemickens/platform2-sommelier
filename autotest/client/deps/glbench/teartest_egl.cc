// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "egl_stuff.h"
#include "teartest.h"


class PixmapToTextureTestEGL : public Test {
 public:
  PixmapToTextureTestEGL();
  virtual bool Start();
  virtual bool Loop(int shift);
  virtual void Stop();

 private:
  Pixmap pixmap_;

  PFNEGLCREATEIMAGEKHRPROC egl_create_image_khr_;
  PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image_khr_;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gl_egl_image_target_texture_2d_oes_;
  EGLImageKHR egl_image_;
};

PixmapToTextureTestEGL::PixmapToTextureTestEGL()
    : egl_image_(EGL_NO_IMAGE_KHR) {
}

bool PixmapToTextureTestEGL::Start() {
  printf("# Attaching pixmap to EGL texture.\n");

  // Get extension entrypoints
  egl_create_image_khr_ =
      reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
      eglGetProcAddress("eglCreateImageKHR"));
  if (!egl_create_image_khr_)
    return false;
  egl_destroy_image_khr_ = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
      eglGetProcAddress("eglDestroyImageKHR"));
  if (!egl_destroy_image_khr_)
    return false;
  gl_egl_image_target_texture_2d_oes_ =
      reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
      eglGetProcAddress("glEGLImageTargetTexture2DOES"));
  if (!gl_egl_image_target_texture_2d_oes_)
    return false;

  pixmap_ = AllocatePixmap();

  static const EGLint egl_image_attributes[] = {
    EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
    EGL_NONE
  };

  egl_image_ = egl_create_image_khr_(
      g_egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
      reinterpret_cast<EGLClientBuffer>(pixmap_), egl_image_attributes);
  if (egl_image_ == EGL_NO_IMAGE_KHR)
    return false;
  gl_egl_image_target_texture_2d_oes_(GL_TEXTURE_2D,
                                      static_cast<GLeglImageOES>(egl_image_));

  return true;
}

bool PixmapToTextureTestEGL::Loop(int shift) {
  UpdatePixmap(pixmap_, shift);
  return true;
}

void PixmapToTextureTestEGL::Stop() {
  egl_destroy_image_khr_(g_egl_display, egl_image_);
}

Test* GetPixmapToTextureTestEGL() {
  return new PixmapToTextureTestEGL();
}
