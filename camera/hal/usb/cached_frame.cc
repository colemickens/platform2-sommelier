/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/cached_frame.h"

#include <errno.h>
#include <libyuv.h>

#include "arc/common.h"
#include "hal/usb/common_types.h"

namespace arc {

CachedFrame::CachedFrame()
    : cropped_buffer_capacity_(0), yu12_buffer_capacity_(0) {
  memset(&source_frame_, 0, sizeof(source_frame_));
  memset(&yu12_frame_, 0, sizeof(yu12_frame_));
}

CachedFrame::~CachedFrame() {}

int CachedFrame::SetSource(const FrameBuffer& frame, int rotate_degree) {
  source_frame_ = frame;
  int res = ConvertToYU12();
  if (res != 0) {
    return res;
  }

  if (rotate_degree >= 0) {
    res = CropRotateScale(rotate_degree);
  }
  return res;
}

void CachedFrame::UnsetSource() {
  memset(&source_frame_, 0, sizeof(source_frame_));
}

uint8_t* CachedFrame::GetSourceBuffer() const {
  return source_frame_.data;
}

size_t CachedFrame::GetSourceDataSize() const {
  return source_frame_.data_size;
}

uint32_t CachedFrame::GetSourceFourCC() const {
  return source_frame_.fourcc;
}

uint8_t* CachedFrame::GetCachedBuffer() const {
  return yu12_frame_.data;
}

uint32_t CachedFrame::GetCachedFourCC() const {
  return yu12_frame_.fourcc;
}

int CachedFrame::GetWidth() const {
  return yu12_frame_.width;
}

int CachedFrame::GetHeight() const {
  return yu12_frame_.height;
}

size_t CachedFrame::GetConvertedSize(uint32_t hal_pixel_format,
                                     int stride) const {
  return ImageProcessor::GetConvertedSize(yu12_frame_, hal_pixel_format,
                                          stride);
}

int CachedFrame::Convert(uint32_t hal_pixel_format,
                         void* output_buffer,
                         size_t output_buffer_size,
                         int output_stride,
                         bool video_hack) {
  if (video_hack && hal_pixel_format == HAL_PIXEL_FORMAT_YV12) {
    hal_pixel_format = CUSTOM_PIXEL_FORMAT_YU12;
  }
  return ImageProcessor::Convert(yu12_frame_, hal_pixel_format, output_buffer,
                                 output_buffer_size, output_stride);
}

int CachedFrame::ConvertToYU12() {
  size_t cache_size = ImageProcessor::GetConvertedSize(
      source_frame_, CUSTOM_PIXEL_FORMAT_YU12, 0);
  if (cache_size == 0) {
    return -EINVAL;
  } else if (cache_size > yu12_buffer_capacity_) {
    yu12_buffer_.reset(new uint8_t[cache_size]);
    yu12_buffer_capacity_ = cache_size;
  }
  yu12_frame_.data = yu12_buffer_.get();
  yu12_frame_.data_size = cache_size;
  yu12_frame_.width = source_frame_.width;
  yu12_frame_.height = source_frame_.height;
  yu12_frame_.fourcc = V4L2_PIX_FMT_YUV420;

  int res = ImageProcessor::Convert(source_frame_, CUSTOM_PIXEL_FORMAT_YU12,
                                    yu12_frame_.data, cache_size, 0);
  if (res) {
    LOGF(ERROR) << "Convert from FOURCC 0x" << std::hex << source_frame_.fourcc
                << " to YU12 fails.";
  }
  return res;
}

int CachedFrame::CropRotateScale(int rotate_degree) {
  if (yu12_frame_.height % 2 != 0 || yu12_frame_.width % 2 != 0) {
    LOGF(ERROR) << "yu12_frame_ has odd dimension: " << yu12_frame_.width << "x"
                << yu12_frame_.height;
    return -EINVAL;
  }

  if (yu12_frame_.height > yu12_frame_.width) {
    LOGF(ERROR) << "yu12_frame_ is tall frame already: " << yu12_frame_.width
                << "x" << yu12_frame_.height;
    return -EINVAL;
  }

  // Step 1: Crop and rotate
  //
  //   Original frame                  Cropped frame              Rotated frame
  // --------------------               --------
  // |     |      |     |               |      |                 ---------------
  // |     |      |     |               |      |                 |             |
  // |     |      |     |   =======>>   |      |     =======>>   |             |
  // |     |      |     |               |      |                 ---------------
  // |     |      |     |               |      |
  // --------------------               --------
  //
  int cropped_width =
      yu12_frame_.height * yu12_frame_.height / yu12_frame_.width;
  if (cropped_width % 2 == 1) {
    // Make cropped_width to the closest even number.
    cropped_width++;
  }
  int cropped_height = yu12_frame_.height;
  int margin = (yu12_frame_.width - cropped_width) / 2;

  int rotated_height = cropped_width;
  int rotated_width = cropped_height;

  int rotated_y_stride = rotated_width;
  int rotated_uv_stride = rotated_width / 2;
  size_t rotated_size =
      rotated_y_stride * rotated_height + rotated_uv_stride * rotated_height;
  if (rotated_size > cropped_buffer_capacity_) {
    cropped_buffer_.reset(new uint8_t[rotated_size]);
    cropped_buffer_capacity_ = rotated_size;
  }
  uint8_t* rotated_y_plane = cropped_buffer_.get();
  uint8_t* rotated_u_plane =
      rotated_y_plane + rotated_y_stride * rotated_height;
  uint8_t* rotated_v_plane =
      rotated_u_plane + rotated_uv_stride * rotated_height / 2;
  libyuv::RotationMode rotation_mode = libyuv::RotationMode::kRotate90;
  switch (rotate_degree) {
    case 90:
      rotation_mode = libyuv::RotationMode::kRotate90;
      break;
    case 270:
      rotation_mode = libyuv::RotationMode::kRotate270;
      break;
    default:
      LOGF(ERROR) << "Invalid rotation degree: " << rotate_degree;
      return -EINVAL;
  }
  // This libyuv method first crops the frame and then rotates it 90 degrees
  // clockwise.
  int res = libyuv::ConvertToI420(
      yu12_frame_.data, yu12_frame_.data_size, rotated_y_plane,
      rotated_y_stride, rotated_u_plane, rotated_uv_stride, rotated_v_plane,
      rotated_uv_stride, margin, 0, yu12_frame_.width, yu12_frame_.height,
      cropped_width, cropped_height, rotation_mode,
      libyuv::FourCC::FOURCC_I420);

  if (res) {
    LOGF(ERROR) << "ConvertToI420 failed: " << res;
    return res;
  }

  // Step 2: Scale
  //
  //                               Final frame
  //  Rotated frame            ---------------------
  // --------------            |                   |
  // |            |  =====>>   |                   |
  // |            |            |                   |
  // --------------            |                   |
  //                           |                   |
  //                           ---------------------
  //
  //
  res = libyuv::I420Scale(
      rotated_y_plane, rotated_y_stride, rotated_u_plane, rotated_uv_stride,
      rotated_v_plane, rotated_uv_stride, rotated_width, rotated_height,
      yu12_frame_.data, yu12_frame_.width,
      yu12_frame_.data + yu12_frame_.width * yu12_frame_.height,
      yu12_frame_.width / 2,
      yu12_frame_.data + yu12_frame_.width * yu12_frame_.height * 5 / 4,
      yu12_frame_.width / 2, yu12_frame_.width, yu12_frame_.height,
      libyuv::FilterMode::kFilterNone);
  LOGF_IF(ERROR, res) << "I420Scale failed: " << res;
  return res;
}

}  // namespace arc
