/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/stream_format.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include <linux/videodev2.h>
#include <system/graphics.h>

#include "cros-camera/common.h"

namespace cros {

static std::vector<int> kSupportedHalFormats{
    HAL_PIXEL_FORMAT_BLOB, HAL_PIXEL_FORMAT_YCbCr_420_888,
    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED};

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

std::vector<int32_t> GetJpegAvailableThumbnailSizes(
    const SupportedFormats& supported_formats) {
  // This list will include at least one non-zero resolution, plus (0,0) for
  // indicating no thumbnail should be generated.
  std::vector<Size> sizes = {{0, 0}};

  // Each output JPEG size in android.scaler.availableStreamConfigurations will
  // have at least one corresponding size that has the same aspect ratio in
  // availableThumbnailSizes, and vice versa.
  for (auto& supported_format : supported_formats) {
    double aspect_ratio =
        static_cast<double>(supported_format.width) / supported_format.height;

    // Note that we only support to generate thumbnails with (width % 8 == 0)
    // and (height % 2 == 0) for now, so set width as multiple of 32 is good for
    // the two common ratios 4:3 and 16:9. When width is 192, the thumbnail
    // sizes would be 192x144 and 192x108 respectively.
    uint32_t thumbnail_width = 192;
    uint32_t thumbnail_height = round(thumbnail_width / aspect_ratio);
    sizes.push_back({thumbnail_width, thumbnail_height});
  }

  // The sizes will be sorted by increasing pixel area (width x height). If
  // several resolutions have the same area, they will be sorted by increasing
  // width.
  std::sort(sizes.begin(), sizes.end());
  sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

  // The aspect ratio of the largest thumbnail size will be same as the aspect
  // ratio of largest JPEG output size in
  // android.scaler.availableStreamConfigurations. The largest size is defined
  // as the size that has the largest pixel area in a given size list.
  auto max_format = GetMaximumFormat(supported_formats);
  double aspect_ratio =
      static_cast<double>(max_format.width) / max_format.height;
  for (uint32_t thumbnail_width = 224; true; thumbnail_width += 32) {
    uint32_t thumbnail_height = round(thumbnail_width / aspect_ratio);
    Size size(thumbnail_width, thumbnail_height);
    if (sizes.back() < size) {
      sizes.push_back(size);
      break;
    }
  }

  std::vector<int32_t> ret;
  for (auto& size : sizes) {
    ret.push_back(size.width);
    ret.push_back(size.height);
  }
  return ret;
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

bool IsFormatSupported(const SupportedFormats& supported_formats,
                       const camera3_stream_t& stream) {
  if (std::find(kSupportedHalFormats.begin(), kSupportedHalFormats.end(),
                stream.format) == kSupportedHalFormats.end()) {
    return false;
  }
  for (const auto& supported_format : supported_formats) {
    if (stream.width == supported_format.width &&
        stream.height == supported_format.height) {
      return true;
    }
  }
  return false;
}

float GetMaximumFrameRate(const SupportedFormat& format) {
  float max_fps = 0;
  for (const auto& frame_rate : format.frame_rates) {
    if (frame_rate > max_fps) {
      max_fps = frame_rate;
    }
  }

  return max_fps;
}

}  // namespace cros
