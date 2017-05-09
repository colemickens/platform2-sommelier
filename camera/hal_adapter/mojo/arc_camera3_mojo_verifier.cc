/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/mojo/arc_camera3.mojom.h"
#include "hardware/camera3.h"

namespace arc {

namespace {

#define CHECK_MOJOM_DEFINITION(name, enum_class)                        \
  static_assert(name == static_cast<int>(arc::mojom::enum_class::name), \
                "Definition of " #name                                  \
                " is inconsistent between mojom and Android framework");

// We must make sure the HAL pixel format definitions in mojom and from Android
// framework are consistent.
#define CHECK_FORMAT_DEFINITION(format) \
  CHECK_MOJOM_DEFINITION(format, HalPixelFormat)

CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_RGBA_8888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_RGBX_8888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_BGRA_8888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YCrCb_420_SP);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YCbCr_422_I);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_BLOB);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YCbCr_420_888);
CHECK_FORMAT_DEFINITION(HAL_PIXEL_FORMAT_YV12);

#define CHECK_ERROR_MSG_CODE_DEFINITION(code) \
  CHECK_MOJOM_DEFINITION(code, Camera3ErrorMsgCode)

CHECK_ERROR_MSG_CODE_DEFINITION(CAMERA3_MSG_ERROR_DEVICE);
CHECK_ERROR_MSG_CODE_DEFINITION(CAMERA3_MSG_ERROR_REQUEST);
CHECK_ERROR_MSG_CODE_DEFINITION(CAMERA3_MSG_ERROR_RESULT);
CHECK_ERROR_MSG_CODE_DEFINITION(CAMERA3_MSG_ERROR_BUFFER);
CHECK_ERROR_MSG_CODE_DEFINITION(CAMERA3_MSG_NUM_ERRORS);

}  // namespace

}  // namespace arc
