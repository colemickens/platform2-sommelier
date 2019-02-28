// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_LIBVDA_GBM_UTIL_H_
#define ARC_VM_LIBVDA_GBM_UTIL_H_

#include <stdint.h>

#include <memory>

#include <gbm.h>

#include "arc/vm/libvda/libvda.h"

namespace arc {

struct GbmDeviceDeleter {
  void operator()(gbm_device* device) { gbm_device_destroy(device); }
};
using ScopedGbmDevicePtr = std::unique_ptr<gbm_device, GbmDeviceDeleter>;

struct GbmBoDeleter {
  void operator()(gbm_bo* bo) { gbm_bo_destroy(bo); }
};
using ScopedGbmBoPtr = std::unique_ptr<gbm_bo, GbmBoDeleter>;

// Converts from libvda's pixel format to GBM format.
uint32_t ConvertPixelFormatToGbmFormat(vda_pixel_format_t format);

}  // namespace arc

#endif  // ARC_VM_LIBVDA_GBM_UTIL_H_
