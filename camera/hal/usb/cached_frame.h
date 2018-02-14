/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_CACHED_FRAME_H_
#define HAL_USB_CACHED_FRAME_H_

#include <memory>

#include <camera/camera_metadata.h>

#include "hal/usb/image_processor.h"

namespace cros {

// CachedFrame contains a source FrameBuffer and a cached, converted
// FrameBuffer. The incoming frames would be converted to YU12, the default
// format of libyuv, to allow convenient processing.
class CachedFrame {
 public:
  CachedFrame();
  ~CachedFrame();

  // SetSource() doesn't take ownership of |frame|. The caller can only release
  // |frame| after calling UnsetSource(). SetSource() immediately converts
  // incoming frame into YU12. Return non-zero values if it encounters errors.
  // If |rotate_degree| is 90 or 270, |frame| will be cropped, rotated by the
  // specified amount and scaled.
  // This function will return an error if |rotate_degree| is not 0, 90, or
  // 270.
  int SetSource(const FrameBuffer* frame, int rotate_degree, bool test_pattern);
  void UnsetSource();

  uint8_t* GetSourceBuffer() const;
  size_t GetSourceDataSize() const;
  uint32_t GetSourceFourCC() const;
  uint8_t* GetCachedBuffer() const;
  uint32_t GetCachedFourCC() const;

  int GetWidth() const;
  int GetHeight() const;

  // Caller should fill everything except |data_size| and |fd| of |out_frame|.
  // The function will do format conversion and scale to fit |out_frame|
  // requirement.
  // If |video_hack| is true, it outputs YU12 when |hal_pixel_format| is YV12
  // (swapping U/V planes). Caller should fill |fourcc|, |data|, and
  // Return non-zero error code on failure; return 0 on success.
  int Convert(const android::CameraMetadata& metadata,
              FrameBuffer* out_frame,
              bool video_hack = false);

 private:
  int ConvertToYU12(bool test_pattern);

  // When we have a landscape mounted camera and the current camera activity is
  // portrait, the frames shown in the activity would be stretched. Therefore,
  // we want to simulate a native portrait camera. That's why we want to crop,
  // rotate |rotate_degree| clockwise and scale the frame. HAL would not change
  // CameraInfo.orientation. Instead, framework would fake the
  // CameraInfo.orientation. Framework would then tell HAL how much the frame
  // needs to rotate clockwise by |rotate_degree|.
  int CropRotateScale(int rotate_degree);

  // CachedFrame does not take the ownership of |source_frame_|.
  const FrameBuffer* source_frame_;

  // Temporary buffer for cropped and rotated results.
  std::unique_ptr<AllocatedFrameBuffer> rotated_frame_;

  // Cache YU12 decoded results.
  std::unique_ptr<AllocatedFrameBuffer> yu12_frame_;
};

}  // namespace cros

#endif  // HAL_USB_CACHED_FRAME_H_
