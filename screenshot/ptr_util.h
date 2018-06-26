// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCREENSHOT_PTR_UTIL_H_
#define SCREENSHOT_PTR_UTIL_H_

#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>

namespace screenshot {

struct DrmModeResDeleter {
  void operator()(drmModeRes* resources) {
    drmModeFreeResources(resources);
  }
};
using ScopedDrmModeResPtr = std::unique_ptr<drmModeRes, DrmModeResDeleter>;

struct DrmModeCrtcDeleter {
  void operator()(drmModeCrtc* crtc) {
    drmModeFreeCrtc(crtc);
  }
};
using ScopedDrmModeCrtcPtr = std::unique_ptr<drmModeCrtc, DrmModeCrtcDeleter>;

struct DrmModeEncoderDeleter {
  void operator()(drmModeEncoder* encoder) {
    drmModeFreeEncoder(encoder);
  }
};
using ScopedDrmModeEncoderPtr =
    std::unique_ptr<drmModeEncoder, DrmModeEncoderDeleter>;

struct DrmModeConnectorDeleter {
  void operator()(drmModeConnector* connector) {
    drmModeFreeConnector(connector);
  }
};
using ScopedDrmModeConnectorPtr =
    std::unique_ptr<drmModeConnector, DrmModeConnectorDeleter>;

struct DrmModeFBDeleter {
  void operator()(drmModeFB* fb) {
    drmModeFreeFB(fb);
  }
};
using ScopedDrmModeFBPtr = std::unique_ptr<drmModeFB, DrmModeFBDeleter>;

struct GbmDeviceDeleter {
  void operator()(gbm_device* device) {
    gbm_device_destroy(device);
  }
};
using ScopedGbmDevicePtr = std::unique_ptr<gbm_device, GbmDeviceDeleter>;

struct GbmBoDeleter {
  void operator()(gbm_bo* bo) {
    gbm_bo_destroy(bo);
  }
};
using ScopedGbmBoPtr = std::unique_ptr<gbm_bo, GbmBoDeleter>;

}  // namespace screenshot

#endif  // SCREENSHOT_PTR_UTIL_H_
