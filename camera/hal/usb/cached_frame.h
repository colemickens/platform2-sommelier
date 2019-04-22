/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_USB_CACHED_FRAME_H_
#define CAMERA_HAL_USB_CACHED_FRAME_H_

#include <memory>

#include <camera/camera_metadata.h>

#include "cros-camera/camera_metrics.h"
#include "cros-camera/jpeg_compressor.h"
#include "cros-camera/jpeg_decode_accelerator.h"
#include "hal/usb/image_processor.h"

namespace cros {

// CachedFrame contains a source FrameBuffer and a cached, converted
// FrameBuffer. The incoming frames would be converted to YU12, the default
// format of libyuv, to allow convenient processing.
class CachedFrame {
 public:
  CachedFrame();

  // SetSource() converts incoming frame into |yu12_frame_|. Return -EAGAIN if
  // |in_frame| is not a valid frame from camera, or other non-zero values if
  // it encounters internal errors.
  // If |rotate_degree| is 90 or 270, |frame| will be cropped, rotated by the
  // specified amount and scaled.
  // This function will return an error if |rotate_degree| is not 0, 90, or
  // 270.
  int SetSource(const FrameBuffer& in_frame, int rotate_degree);

  // Caller should fill everything except |data_size| and |fd| of |out_frame|.
  // The function will do crop of the center image by |crop_width| and
  // |crop_height| first, and then do format conversion and scale to fit
  // |out_frame| requirement.
  int Convert(const android::CameraMetadata& metadata,
              int crop_width,
              int crop_height,
              FrameBuffer* out_frame);

 private:
  int ConvertToYU12(const FrameBuffer& in_frame);

  int DecodeToYU12ByJDA(const FrameBuffer& in_frame);

  int ConvertYU12ToJpeg(const android::CameraMetadata& metadata,
                        const FrameBuffer& in_frame,
                        FrameBuffer* out_frame);

  // When we have a landscape mounted camera and the current camera activity is
  // portrait, the frames shown in the activity would be stretched. Therefore,
  // we want to simulate a native portrait camera. That's why we want to crop,
  // rotate |rotate_degree| clockwise and scale the frame. HAL would not change
  // CameraInfo.orientation. Instead, framework would fake the
  // CameraInfo.orientation. Framework would then tell HAL how much the frame
  // needs to rotate clockwise by |rotate_degree|.
  int CropRotateScale(int rotate_degree);

  // Temporary buffer for cropped, rotated and scaled results.
  std::unique_ptr<SharedFrameBuffer> temp_frame_;
  std::unique_ptr<SharedFrameBuffer> temp_frame2_;

  // Cache YU12 decoded results.
  std::unique_ptr<SharedFrameBuffer> yu12_frame_;

  // ImageProcessor instance.
  std::unique_ptr<ImageProcessor> image_processor_;

  // JPEG decoder accelerator (JDA) instance
  std::unique_ptr<JpegDecodeAccelerator> jda_;

  // JPEG compressor instance
  std::unique_ptr<JpegCompressor> jpeg_compressor_;

  // Metrics that used to record things like decoding latency.
  std::unique_ptr<CameraMetrics> camera_metrics_;

  // Indicate if JDA started successfully
  bool jda_available_;

  // Flags to disable SW encode/decode fallback when HW encode/decode failed
  bool force_jpeg_hw_encode_;
  bool force_jpeg_hw_decode_;
};

}  // namespace cros

#endif  // CAMERA_HAL_USB_CACHED_FRAME_H_
