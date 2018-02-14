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

#include <camera/camera_metadata.h>

#include "hal/usb/frame_buffer.h"

namespace cros {

// V4L2_PIX_FMT_YVU420(YV12) in ImageProcessor has alignment requirement.
// The stride of Y, U, and V planes should a multiple of 16 pixels.
struct ImageProcessor {
  // Calculate the output buffer size when converting to the specified pixel
  // format according to fourcc, width, height, and stride of |frame|.
  // Return 0 on error.
  static size_t GetConvertedSize(const FrameBuffer& frame);

  // Convert format from |in_frame.fourcc| to |out_frame->fourcc|. Caller should
  // fill |data|, |buffer_size|, |width|, and |height| of |out_frame|. The
  // function will fill |out_frame->data_size|. Return non-zero error code on
  // failure; return 0 on success.
  static int ConvertFormat(const android::CameraMetadata& metadata,
                           const FrameBuffer& in_frame,
                           FrameBuffer* out_frame);

  // Scale image size according to |in_frame| and |out_frame|. Only support
  // V4L2_PIX_FMT_YUV420 format. Caller should fill |data|, |width|, |height|,
  // and |buffer_size| of |out_frame|. The function will fill |data_size| and
  // |fourcc| of |out_frame|.
  static int Scale(const FrameBuffer& in_frame, FrameBuffer* out_frame);

  // Crop and rotate image size according to |in_frame| and |out_frame|. Only
  // support V4L2_PIX_FMT_YUV420 format. Caller should fill |data|, |width|,
  // |height|, and |buffer_size| of |out_frame|. The function will fill
  // |data_size| and |fourcc| of |out_frame|. |rotate_degree| should be 90 or
  // 270.
  static int CropAndRotate(const FrameBuffer& in_frame,
                           FrameBuffer* out_frame,
                           int rotate_degree);
};

}  // namespace cros

#endif  // HAL_USB_IMAGE_PROCESSOR_H_
