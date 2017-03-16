/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/stream_format.h"

#include <linux/videodev2.h>

#include "arc/common.h"
#include "hal/usb/metadata_handler.h"

namespace arc {

static const std::vector<uint32_t> GetSupportedFourCCs() {
  // The preference of supported fourccs in the list is from high to low.
  static const std::vector<uint32_t> kSupportedFourCCs = {
      V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUYV,
  };
  return kSupportedFourCCs;
}

// Return corresponding format by matching resolution |width|x|height| in
// |formats|.
const SupportedFormat* FindFormatByResolution(const SupportedFormats& formats,
                                              uint32_t width,
                                              uint32_t height) {
  for (const auto& format : formats) {
    if (format.width == width && format.height == height) {
      return &format;
    }
  }
  return NULL;
}

SupportedFormat GetMaximumFormat(const SupportedFormats& supported_formats) {
  SupportedFormat max_format;
  memset(&max_format, 0, sizeof(max_format));
  for (const auto& supported_format : supported_formats) {
    if (supported_format.width >= max_format.width &&
        supported_format.height >= max_format.height) {
      max_format = supported_format;
    }
  }
  return max_format;
}

SupportedFormats GetQualifiedFormats(
    const SupportedFormats& supported_formats) {
  // The preference of supported fourccs in the list is from high to low.
  const std::vector<uint32_t> supported_fourccs = GetSupportedFourCCs();
  SupportedFormats qualified_formats;
  for (const auto& supported_fourcc : supported_fourccs) {
    for (const auto& supported_format : supported_formats) {
      if (supported_format.fourcc != supported_fourcc) {
        continue;
      }

      // Skip if |qualified_formats| already has the same resolution with a more
      // preferred fourcc.
      if (FindFormatByResolution(qualified_formats, supported_format.width,
                                 supported_format.height) != NULL) {
        continue;
      }
      qualified_formats.push_back(supported_format);
    }
  }
  return qualified_formats;
}

int HalPixelFormatToFourcc(uint32_t hal_pixel_format) {
  switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_YV12:
      return V4L2_PIX_FMT_YVU420;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
      return V4L2_PIX_FMT_NV21;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
      return V4L2_PIX_FMT_YUYV;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:  // Fall-through
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return V4L2_PIX_FMT_RGB32;
    case HAL_PIXEL_FORMAT_BLOB:
      return V4L2_PIX_FMT_MJPEG;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      // This is a flexible YUV format that depends on platform. Different
      // platform may have different format. It can be YVU420 or NV12. Now we
      // return YVU420 first.
      // TODO(henryhsu): call drm_drv.get_fourcc() to get correct format.
      return V4L2_PIX_FMT_YVU420;
    default:
      LOGF(ERROR) << "Pixel format 0x" << std::hex << hal_pixel_format
                  << " is unsupported.";
  }
  return -EINVAL;
}

}  // namespace arc
