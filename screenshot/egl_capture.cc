// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screenshot/egl_capture.h"

#include <sys/mman.h>

#include <utility>
#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "screenshot/crtc.h"

namespace screenshot {

EglPixelBuf::EglPixelBuf(ScopedGbmDevicePtr device, std::vector<char> buffer, uint32_t x,
                         uint32_t y, uint32_t width, uint32_t height, uint32_t stride)
    : device_(std::move(device)), width_(width), height_(height),
      stride_(stride), buffer_(buffer) {
}

std::unique_ptr<EglPixelBuf> EglCapture(const Crtc& crtc, uint32_t x, uint32_t y,
                                        uint32_t width, uint32_t height) {
  ScopedGbmDevicePtr device(gbm_create_device(crtc.file().GetPlatformFile()));
  CHECK(device) << "gbm_create_device failed";

  EGLDisplay display = eglGetDisplay(device.get());
  CHECK(display != EGL_NO_DISPLAY) << "Could not get EGLDisplay";

  EGLBoolean egl_ret = eglInitialize(display, NULL, NULL);
  CHECK(egl_ret) << "Could not initialize EGLDisplay";

  const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_DONT_CARE,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  EGLint num_configs;
  EGLConfig config;

  egl_ret = eglChooseConfig(display, config_attribs, &config, 1, &num_configs);
  CHECK(egl_ret) << "Could not choose EGLConfig";

  EGLContext ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
  CHECK(ctx != EGL_NO_CONTEXT) << "Could not create EGLContext";

  egl_ret = eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
  CHECK(egl_ret) << "Could not bind context";

  std::string egl_extensions = eglQueryString(display, EGL_EXTENSIONS);
  CHECK(egl_extensions.find("EGL_KHR_image_base") != std::string::npos)
      << "Missing EGL extension: EGL_KHR_image_base";
  CHECK(egl_extensions.find("EGL_EXT_image_dma_buf_import") != std::string::npos)
      << "Missing EGL extension: EGL_EXT_image_dma_buf_import";

  std::string gl_extensions = (const char *) glGetString(GL_EXTENSIONS);
  CHECK(gl_extensions.find("GL_OES_EGL_image") != std::string::npos)
      << "Missing GL extension: GL_OES_EGL_image";
  CHECK(gl_extensions.find("GL_OES_EGL_image_external") != std::string::npos)
      << "Missing GL extension: GL_OES_EGL_image_external";

  int num_planes = 0;
  base::ScopedFD fds[GBM_MAX_PLANES] = { };
  for (size_t plane = 0; plane < GBM_MAX_PLANES; plane++) {
    // getfb2() doesn't return the number of planes so get handles
    // and count planes until we find a handle that isn't set
    if (crtc.fb2()->handles[plane] == 0)
      break;

    int fd;
    int ret = drmPrimeHandleToFD(crtc.file().GetPlatformFile(),
                                 crtc.fb2()->handles[plane], 0, &fd);
    CHECK_EQ(ret, 0) << "drmPrimeHandleToFD failed";
    fds[plane].reset(fd);
    num_planes++;
  }

  CHECK_GT(num_planes, 0);

  EGLint attr_list[46] = {
    EGL_WIDTH, static_cast<EGLint>(crtc.fb2()->width),
    EGL_HEIGHT, static_cast<EGLint>(crtc.fb2()->height),
    EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(crtc.fb2()->pixel_format),
  };

  size_t attrs_index = 6;

  for (size_t plane = 0; plane < num_planes; plane++) {
    attr_list[attrs_index++] = EGL_DMA_BUF_PLANE0_FD_EXT + plane * 3;
    attr_list[attrs_index++] = fds[plane].get();
    attr_list[attrs_index++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT + plane * 3;
    attr_list[attrs_index++] = crtc.fb2()->offsets[plane];
    attr_list[attrs_index++] = EGL_DMA_BUF_PLANE0_PITCH_EXT + plane * 3;
    attr_list[attrs_index++] = crtc.fb2()->pitches[plane];
    attr_list[attrs_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + plane * 2;
    attr_list[attrs_index++] = crtc.fb2()->modifier & 0xfffffffful;
    attr_list[attrs_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT + plane * 2;
    attr_list[attrs_index++] = crtc.fb2()->modifier >> 32;
  }

  attr_list[attrs_index] = EGL_NONE;

  PFNEGLCREATEIMAGEKHRPROC CreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  CHECK(CreateImageKHR) << "CreateImageKHR not supported";

  PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  CHECK(DestroyImageKHR) << "DestroyImageKHR not supported";

  EGLImageKHR image = CreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, attr_list);
  CHECK(image != EGL_NO_IMAGE_KHR) << "Failed to create image";

  unsigned int fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  unsigned int texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  CHECK(glEGLImageTargetTexture2DOES) << "glEGLImageTargetTexture2DOES not supported";

  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) <<  "fb did not complete";

  std::vector<char> buffer(crtc.fb2()->pitches[0] * crtc.fb2()->height);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(x, y, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &buffer[0]);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  DestroyImageKHR(display, image);
  glDeleteTextures(1, &texture);
  glDeleteFramebuffers(1, &fbo);
  eglDestroyContext(display, ctx);
  eglTerminate(display);

  return std::make_unique<EglPixelBuf>(std::move(device), buffer, x, y,
                                       width, height, width * 4);
}

}  // namespace screenshot
