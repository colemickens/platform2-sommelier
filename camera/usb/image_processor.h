/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef USB_IMAGE_PROCESSOR_H_
#define USB_IMAGE_PROCESSOR_H_

#include <stdint.h>

#include <string>
#include <vector>

// FourCC pixel formats (defined as V4L2_PIX_FMT_*).
#include <linux/videodev2.h>
// Declarations of HAL_PIXEL_FORMAT_XXX.
#include <system/graphics.h>

#include "usb/frame_buffer.h"

namespace arc {

struct ImageProcessor {
  // Calculate the output buffer size when converting to the specified pixel
  // format. |hal_pixel_format| is defined as HAL_PIXEL_FORMAT_XXX in
  // /system/core/include/system/graphics.h. If |stride| is non-zero, use it as
  // the byte stride for RGBA destination buffer. Return 0 on error.
  static size_t GetConvertedSize(const FrameBuffer& frame,
                                 uint32_t hal_pixel_format,
                                 int stride);

  // If |output_stride| is non-zero, use it as the byte stride of
  // |output_buffer|. Return non-zero error code on failure; return 0 on
  // success.
  static int Convert(const FrameBuffer& frame,
                     uint32_t hal_pixel_format,
                     void* output_buffer,
                     size_t output_buffer_size,
                     int output_stride);

  // Get all supported source pixel formats of conversion in fourcc.
  static const std::vector<uint32_t> GetSupportedFourCCs();
};

}  // namespace arc

#endif  // USB_IMAGE_PROCESSOR_H_
