/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_IMAGE_PROCESSOR_H_
#define HAL_USB_IMAGE_PROCESSOR_H_

#include <memory>
#include <string>

// FourCC pixel formats (defined as V4L2_PIX_FMT_*).
#include <linux/videodev2.h>
// Declarations of HAL_PIXEL_FORMAT_XXX.
#include <system/graphics.h>

#include <base/memory/ptr_util.h>
#include <camera/camera_metadata.h>

#include "cros-camera/jpeg_compressor.h"
#include "cros-camera/jpeg_decode_accelerator.h"
#include "hal/usb/frame_buffer.h"

namespace cros {

// V4L2_PIX_FMT_YVU420(YV12) in ImageProcessor has alignment requirement.
// The stride of Y, U, and V planes should a multiple of 16 pixels.
class ImageProcessor {
 public:
  ImageProcessor();
  ~ImageProcessor();

  // Calculate the output buffer size when converting to the specified pixel
  // format according to fourcc, width, height, and stride of |frame|.
  // Return 0 on error.
  size_t GetConvertedSize(const FrameBuffer& frame);

  // Convert format from |in_frame.fourcc| to |out_frame->fourcc|. Caller should
  // fill |data|, |buffer_size|, |width|, and |height| of |out_frame|. The
  // function will fill |out_frame->data_size|. Return non-zero error code on
  // failure; return 0 on success.
  int ConvertFormat(const android::CameraMetadata& metadata,
                    const FrameBuffer& in_frame,
                    FrameBuffer* out_frame);

  // Scale image size according to |in_frame| and |out_frame|. Only support
  // V4L2_PIX_FMT_YUV420 format. Caller should fill |data|, |width|, |height|,
  // and |buffer_size| of |out_frame|. The function will fill |data_size| and
  // |fourcc| of |out_frame|.
  int Scale(const FrameBuffer& in_frame, FrameBuffer* out_frame);

  // Crop and rotate image size according to |in_frame| and |out_frame|. Only
  // support V4L2_PIX_FMT_YUV420 format. Caller should fill |data|, |width|,
  // |height|, and |buffer_size| of |out_frame|. The function will fill
  // |data_size| and |fourcc| of |out_frame|. |rotate_degree| should be 90 or
  // 270.
  int ProcessForInsetPortraitMode(const FrameBuffer& in_frame,
                                  FrameBuffer* out_frame,
                                  int rotate_degree);

  // Crop image size according to |in_frame| and |out_frame|. Only
  // support V4L2_PIX_FMT_YUV420 format. Caller should fill |data|, |width|,
  // |height|, and |buffer_size| of |out_frame|. The function will fill
  // |data_size| and |fourcc| of |out_frame|.
  int Crop(const FrameBuffer& in_frame, FrameBuffer* out_frame);

 private:
  bool ConvertToJpeg(const android::CameraMetadata& metadata,
                     const FrameBuffer& in_frame,
                     FrameBuffer* out_frame);

  void InsertJpegBlob(FrameBuffer* out_frame, uint32_t jpeg_data_size);

  int MJPGToI420(const FrameBuffer& in_frame, FrameBuffer* out_frame);

  // Used for jpeg decode accelerator.
  std::unique_ptr<JpegDecodeAccelerator> jda_;
  // Indicate if we can use the jpeg decoder accelerator or not.
  bool jda_available_;

  std::unique_ptr<JpegCompressor> jpeg_compressor_;

  // Set in hardware test for disable fallback to SW encode/decode when HW
  // encode/decode failed.
  bool force_jpeg_hw_encode_;
  bool force_jpeg_hw_decode_;
};

}  // namespace cros

#endif  // HAL_USB_IMAGE_PROCESSOR_H_
