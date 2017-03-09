/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_CACHED_FRAME_H_
#define HAL_USB_CACHED_FRAME_H_

#include <memory>

#include "hal/usb/image_processor.h"

namespace arc {

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
  // If |rotate_degree| is -1, |frame| will not be cropped, rotated, and scaled.
  // This function will return an error if |rotate_degree| is not -1, 90, or
  // 270.
  int SetSource(const FrameBuffer& frame, int rotate_degree);
  void UnsetSource();

  uint8_t* GetSourceBuffer() const;
  size_t GetSourceDataSize() const;
  uint32_t GetSourceFourCC() const;
  uint8_t* GetCachedBuffer() const;
  uint32_t GetCachedFourCC() const;

  int GetWidth() const;
  int GetHeight() const;

  // Calculate the output buffer size when converting to the specified pixel
  // format. |hal_pixel_format| is defined as HAL_PIXEL_FORMAT_XXX in
  // /system/core/include/system/graphics.h. If |stride| is non-zero, use it as
  // the byte stride for destination buffer. Return 0 on error.
  size_t GetConvertedSize(uint32_t hal_pixel_format, int stride) const;

  // Return non-zero error code on failure; return 0 on success. If
  // |output_stride| is non-zero, use it as the byte stride of |output_buffer|.
  // If |video_hack| is true, it outputs YU12 when |hal_pixel_format| is YV12
  // (swapping U/V planes).
  int Convert(uint32_t hal_pixel_format,
              void* output_buffer,
              size_t output_buffer_size,
              int output_stride,
              bool video_hack = false);

 private:
  int ConvertToYU12();
  // When we have a landscape mounted camera and the current camera activity is
  // portrait, the frames shown in the activity would be stretched. Therefore,
  // we want to simulate a native portrait camera. That's why we want to crop,
  // rotate |rotate_degree| clockwise and scale the frame. HAL would not change
  // CameraInfo.orientation. Instead, framework would fake the
  // CameraInfo.orientation. Framework would then tell HAL how much the frame
  // needs to rotate clockwise by |rotate_degree|.
  int CropRotateScale(int rotate_degree);

  FrameBuffer source_frame_;

  // Temporary buffer for cropped and rotated results.
  std::unique_ptr<uint8_t[]> cropped_buffer_;
  size_t cropped_buffer_capacity_;

  // Cache YU12 decoded results.
  FrameBuffer yu12_frame_;
  std::unique_ptr<uint8_t[]> yu12_buffer_;
  size_t yu12_buffer_capacity_;
};

}  // namespace arc

#endif  // HAL_USB_CACHED_FRAME_H_
