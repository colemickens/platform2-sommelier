/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb/captured_frame.h"

#include <errno.h>
#include <libyuv.h>

#include "arc/common.h"
#include "usb/common_types.h"

namespace arc {

/*
 * Formats have different names in different header files. Here is the mapping
 * table:
 *
 * android_pixel_format_t          videodev2.h           FOURCC in libyuv
 * -----------------------------------------------------------------------------
 * HAL_PIXEL_FORMAT_YV12         = V4L2_PIX_FMT_YVU420 = FOURCC_YV12
 * CUSTOM_PIXEL_FORMAT_YU12      = V4L2_PIX_FMT_YUV420 = FOURCC_I420
 *                                                     = FOURCC_YU12
 * HAL_PIXEL_FORMAT_YCrCb_420_SP = V4L2_PIX_FMT_NV21   = FOURCC_NV21
 * HAL_PIXEL_FORMAT_BGRA_8888    = V4L2_PIX_FMT_BGR32  = FOURCC_ARGB
 * HAL_PIXEL_FORMAT_YCbCr_422_I  = V4L2_PIX_FMT_YUYV   = FOURCC_YUYV
 *                                                     = FOURCC_YUY2
 *                                 V4L2_PIX_FMT_MJPEG  = FOURCC_MJPG
 *
 * YU12 is not defined in android_pixel_format_t. So we define it as
 * CUSTOM_PIXEL_FORMAT_YU12 in CommonTypes.h.
 *
 * Camera device generates FOURCC_YUYV and FOURCC_MJPG.
 * Preview needs FOURCC_ARGB format.
 * Software video encoder needs FOURCC_YU12.
 * CTS requires FOURCC_YV12 and FOURCC_NV21 for applications.
 *
 * Android stride requirement:
 * YV12 horizontal stride should be a multiple of 16 pixels. See
 * android.graphics.ImageFormat.YV12.
 * ARGB can have a stride equal or bigger than the width.
 * The stride of YU12 and NV21 is always equal to the width.
 *
 * Conversion Path:
 * MJPG/YUYV (from camera) -> YU12 -> ARGB (preview)
 *                                 -> NV21 (apps)
 *                                 -> YV12 (apps)
 *                                 -> YU12 (video encoder)
 */

// YV12 horizontal stride should be a multiple of 16 pixels for each plane.
// |dst_stride_uv| is the pixel stride of u or v plane.
static int YU12ToYV12(const void* yv12,
                      void* yu12,
                      int width,
                      int height,
                      int dst_stride_y,
                      int dst_stride_uv);
static int YU12ToNV21(const void* yv12, void* nv21, int width, int height);

inline static size_t Align16(size_t value) {
  return (value + 15) & ~15;
}

size_t CapturedFrame::GetConvertedSize(const CapturedFrame& frame,
                                       uint32_t hal_pixel_format,
                                       int stride) {
  if ((frame.width % 2) || (frame.height % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << frame.width << " x "
                << frame.height << ")";
    return 0;
  }

  if (stride) {
    if (hal_pixel_format == HAL_PIXEL_FORMAT_BGRA_8888) {
      return stride * frame.height;
    } else {
      // Single stride value doesn't apply to YUV formats.
      LOGF(ERROR) << "Stride is unsupported for pixel format 0x" << std::hex
                  << hal_pixel_format;
      return 0;
    }
  }

  switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_YV12:  // YV12
      return Align16(frame.width) * frame.height +
             Align16(frame.width / 2) * frame.height;
    case CUSTOM_PIXEL_FORMAT_YU12:  // YU12
    // Fall-through.
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:  // NV21
      return frame.width * frame.height * 3 / 2;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return frame.width * frame.height * 4;
    default:
      LOGF(ERROR) << "Pixel format 0x" << std::hex << hal_pixel_format
                  << " is unsupported.";
      return 0;
  }
}

int CapturedFrame::Convert(const CapturedFrame& frame,
                           uint32_t hal_pixel_format,
                           void* output_buffer,
                           size_t output_buffer_size,
                           int output_stride) {
  if ((frame.width % 2) || (frame.height % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << frame.width << " x "
                << frame.height << ")";
    return 0;
  }

  if (output_buffer_size !=
      GetConvertedSize(frame, hal_pixel_format, output_stride)) {
    LOGF(ERROR) << "Buffer overflow: output buffer has incorrect size ("
                << output_buffer_size << ") for " << frame.width << "x"
                << frame.height << " frame (stride=" << output_stride << ").";
    return -EINVAL;
  }
  if (hal_pixel_format == HAL_PIXEL_FORMAT_BGRA_8888) {
    if (!output_stride) {
      output_stride = frame.width * 4;
    } else if (output_stride < frame.width * 4) {
      LOGF(ERROR) << "Invalid stride(" << output_stride
                  << ") < 4 * frame width(" << frame.width
                  << ") for BGRA frame.";
      return -EINVAL;
    }
  } else {
    if (output_stride) {
      LOGF(ERROR) << "Destination pixel format 0x" << std::hex
                  << hal_pixel_format << " does not support stride.";
      return -EINVAL;
    }
  }

  if (frame.fourcc == V4L2_PIX_FMT_YUYV) {
    switch (hal_pixel_format) {
      case CUSTOM_PIXEL_FORMAT_YU12:  // YU12
      {
        uint8_t* dst = reinterpret_cast<uint8_t*>(output_buffer);
        int res = libyuv::YUY2ToI420(
            frame.buffer, frame.width * 2, dst, frame.width,
            dst + frame.width * frame.height, frame.width / 2,
            dst + frame.width * frame.height * 5 / 4, frame.width / 2,
            frame.width, frame.height);
        LOGF_IF(ERROR, res) << "YUY2ToI420() for YU12 returns " << res;
        return res ? -EINVAL : 0;
      }
      case HAL_PIXEL_FORMAT_YV12:  // YV12
      // Fall-through.
      case HAL_PIXEL_FORMAT_YCrCb_420_SP:  // NV21
      // Fall-through.
      case HAL_PIXEL_FORMAT_BGRA_8888:
        // Unsupported path. CachedFrame will use an intermediate YU12
        // CapturedFrame for
        // this conversion path.
        return -EINVAL;
      default:
        LOGF(ERROR) << "Destination pixel format " << std::hex
                    << hal_pixel_format
                    << " is unsupported for YUYV source format.";
        return -EINVAL;
    }
  } else if (frame.fourcc == V4L2_PIX_FMT_YUV420) {
    // V4L2_PIX_FMT_YVU420 is YV12. I420 is usually referred to YU12
    // (V4L2_PIX_FMT_YUV420), and
    // YV12 is similar to YU12 except that U/V planes are swapped.
    switch (hal_pixel_format) {
      case HAL_PIXEL_FORMAT_YV12:  // YV12
      {
        int ystride = Align16(frame.width);
        int uvstride = Align16(frame.width / 2);
        int res = YU12ToYV12(frame.buffer, output_buffer, frame.width,
                             frame.height, ystride, uvstride);
        LOGF_IF(ERROR, res) << "YU12ToYV12() returns " << res;
        return res ? -EINVAL : 0;
      }
      case CUSTOM_PIXEL_FORMAT_YU12:  // YU12
      {
        memcpy(output_buffer, frame.buffer, output_buffer_size);
        return 0;
      }
      case HAL_PIXEL_FORMAT_YCrCb_420_SP:  // NV21
      {
        int res =
            YU12ToNV21(frame.buffer, output_buffer, frame.width, frame.height);
        LOGF_IF(ERROR, res) << "YU12ToNV21() returns " << res;
        return res ? -EINVAL : 0;
      }
      case HAL_PIXEL_FORMAT_BGRA_8888: {
        int res = libyuv::I420ToARGB(
            frame.buffer, frame.width,
            frame.buffer + frame.width * frame.height, frame.width / 2,
            frame.buffer + frame.width * frame.height * 5 / 4, frame.width / 2,
            static_cast<uint8_t*>(output_buffer), output_stride, frame.width,
            frame.height);
        LOGF_IF(ERROR, res) << "I420ToARGB() returns " << res;
        return res ? -EINVAL : 0;
      }
      default:
        LOGF(ERROR) << "Destination pixel format " << std::hex
                    << hal_pixel_format
                    << " is unsupported for YU12 source format.";
        return -EINVAL;
    }
  } else if (frame.fourcc == V4L2_PIX_FMT_MJPEG) {
    switch (hal_pixel_format) {
      case CUSTOM_PIXEL_FORMAT_YU12:  // YU12
      {
        uint8_t* yplane = static_cast<uint8_t*>(output_buffer);
        int res = libyuv::MJPGToI420(
            frame.buffer, frame.data_size, yplane, frame.width,
            yplane + frame.width * frame.height, frame.width / 2,
            yplane + frame.width * frame.height * 5 / 4, frame.width / 2,
            frame.width, frame.height, frame.width, frame.height);
        LOGF_IF(ERROR, res) << "MJPEGToI420() returns " << res;
        return res ? -EINVAL : 0;
      }
      case HAL_PIXEL_FORMAT_YV12:  // YV12
      // Fall-through.
      case HAL_PIXEL_FORMAT_YCrCb_420_SP:  // NV21
      // Fall-through.
      case HAL_PIXEL_FORMAT_BGRA_8888:
        // Unsupported path. CachedFrame will use an intermediate YU12
        // CapturedFrame for
        // this conversion path.
        return -EINVAL;
      default:
        LOGF(ERROR) << "Destination pixel format " << std::hex
                    << hal_pixel_format
                    << " is unsupported for MJPEG source format.";
        return -EINVAL;
    }
  } else {
    LOGF(ERROR) << "Convert format doesn't support source format "
                << "FOURCC 0x" << std::hex << frame.fourcc;
    return -EINVAL;
  }
}

static int YU12ToYV12(const void* yu12,
                      void* yv12,
                      int width,
                      int height,
                      int dst_stride_y,
                      int dst_stride_uv) {
  if ((width % 2) || (height % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << width << " x " << height
                << ")";
    return -EINVAL;
  }
  if (dst_stride_y < width || dst_stride_uv < width / 2) {
    LOGF(ERROR) << "Y plane stride (" << dst_stride_y
                << ") or U/V plane stride (" << dst_stride_uv
                << ") is invalid for width " << width;
    return -EINVAL;
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(yu12);
  uint8_t* dst = reinterpret_cast<uint8_t*>(yv12);
  const uint8_t* u_src = src + width * height;
  uint8_t* u_dst = dst + dst_stride_y * height + dst_stride_uv * height / 2;
  const uint8_t* v_src = src + width * height * 5 / 4;
  uint8_t* v_dst = dst + dst_stride_y * height;

  return libyuv::I420Copy(src, width, u_src, width / 2, v_src, width / 2, dst,
                          dst_stride_y, u_dst, dst_stride_uv, v_dst,
                          dst_stride_uv, width, height);
}

static int YU12ToNV21(const void* yu12, void* nv21, int width, int height) {
  if ((width % 2) || (height % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << width << " x " << height
                << ")";
    return -EINVAL;
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(yu12);
  uint8_t* dst = reinterpret_cast<uint8_t*>(nv21);
  const uint8_t* u_src = src + width * height;
  const uint8_t* v_src = src + width * height * 5 / 4;
  uint8_t* vu_dst = dst + width * height;

  memcpy(dst, src, width * height);

  for (int i = 0; i < height / 2; i++) {
    for (int j = 0; j < width / 2; j++) {
      *vu_dst++ = *v_src++;
      *vu_dst++ = *u_src++;
    }
  }
  return 0;
}

const std::vector<uint32_t> CapturedFrame::GetSupportedFourCCs() {
  // The preference of supported fourccs in the list is from high to low.
  static const std::vector<uint32_t> kSupportedFourCCs = {
      V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_YUV420};
  return kSupportedFourCCs;
}

}  // namespace arc
