/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_IMAGE_PROCESSOR_H_
#define HAL_USB_IMAGE_PROCESSOR_H_

#include <string>

// FourCC pixel formats (defined as V4L2_PIX_FMT_*).
#include <linux/videodev2.h>
// Declarations of HAL_PIXEL_FORMAT_XXX.
#include <system/graphics.h>

#include "hal/usb/frame_buffer.h"

namespace arc {

// V4L2_PIX_FMT_YVU420(YV12) in ImageProcessor has alignment requirement.
// The stride of Y, U, and V planes should a multiple of 16 pixels.
struct ImageProcessor {
  // Calculate the output buffer size when converting to the specified pixel
  // format. |fourcc| is defined as V4L2_PIX_FMT_* in linux/videodev2.h.
  // Return 0 on error.
  static size_t GetConvertedSize(int fourcc, uint32_t width, uint32_t height);

  // Convert format from |in_frame.fourcc| to |out_frame->fourcc| and copy
  // width and height of |in_frame| to |out_frame|. Caller should fill |data|
  // and |buffer_size| of |out_frame|. The function will fill
  // |out_frame->data_size|. Return non-zero error code on failure; return 0 on
  // success.
  static int Convert(const FrameBuffer& in_frame, FrameBuffer* out_frame);
};

}  // namespace arc

#endif  // HAL_USB_IMAGE_PROCESSOR_H_
