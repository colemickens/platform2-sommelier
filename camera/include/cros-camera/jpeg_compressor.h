/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_INCLUDE_CROS_CAMERA_JPEG_COMPRESSOR_H_
#define CAMERA_INCLUDE_CROS_CAMERA_JPEG_COMPRESSOR_H_

#include <memory>

#include "cros-camera/export.h"

namespace cros {

// Interface for YU12 to JPEG compressor.
class CROS_CAMERA_EXPORT JpegCompressor {
 public:
  enum class Mode {
    // Do HW encode first; falls back to SW encode after failing.
    kDefault = 0,
    // Don't do HW encode; use SW encode directly.
    kSwOnly,
    // HW encode only; does not fall back to SW encode.
    kHwOnly
  };

  static std::unique_ptr<JpegCompressor> GetInstance();

  virtual ~JpegCompressor() {}

  // Compresses YU12 image to JPEG format with HW encode acceleration. It would
  // fallback to SW encode if HW encode fails by default.
  // |quality| is the resulted jpeg image quality. It ranges from 1
  // (poorest quality) to 100 (highest quality).
  // |app1_buffer| is the buffer of APP1 segment (exif) which will be added to
  // the compressed image. Caller should pass the size of output buffer to
  // |out_buffer_size|. Encoded result will be written into |output_buffer|.
  // The actually encoded size will be written into |out_data_size| if image
  // encoded successfully. Returns false if errors occur during compression.
  // |mode| controls the HW/SW encode selection strategy.
  virtual bool CompressImage(const void* image,
                     int width,
                     int height,
                     int quality,
                     const void* app1_buffer,
                     uint32_t app1_size,
                     uint32_t out_buffer_size,
                     void* out_buffer,
                     uint32_t* out_data_size,
                     Mode mode = Mode::kDefault) = 0;

  // Compresses YU12 image to JPEG format. |quality| is the resulted jpeg
  // image quality. It ranges from 1 (poorest quality) to 100 (highest quality).
  // Caller should pass the size of output buffer to |out_buffer_size|. Encoded
  // result will be written into |output_buffer|. The actually encoded size will
  // be written into |out_data_size| if image encoded successfully. Returns
  // false if errors occur during compression.
  virtual bool GenerateThumbnail(const void* image,
                         int image_width,
                         int image_height,
                         int thumbnail_width,
                         int thumbnail_height,
                         int quality,
                         uint32_t out_buffer_size,
                         void* out_buffer,
                         uint32_t* out_data_size) = 0;
};

}  // namespace cros

#endif  // CAMERA_INCLUDE_CROS_CAMERA_JPEG_COMPRESSOR_H_
