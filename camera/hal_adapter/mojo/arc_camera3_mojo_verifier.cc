/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/mojo/arc_camera3.mojom.h"
#include "hardware/camera3.h"

namespace arc {

namespace {

// We must make sure the HAL pixel format defitions in mojom and from Android
// framework are consistent.
#define CHECK_FORMAT_DEFINITION(format)                               \
  static_assert(                                                      \
      format == static_cast<int>(arc::mojom::HalPixelFormat::format), \
      "Definition of " #format                                        \
      " is inconsistent between mojom and Android framework");

CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_RGBA_8888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_RGBX_8888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_BGRA_8888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YCrCb_420_SP);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YCbCr_422_I);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_BLOB);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YCbCr_420_888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YV12);

}  // namespace

}  // namespace arc
