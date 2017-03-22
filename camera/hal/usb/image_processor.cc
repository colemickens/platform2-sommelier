/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/image_processor.h"

#include <errno.h>
#include <libyuv.h>

#include "arc/common.h"
#include "hal/usb/common_types.h"

namespace arc {

/*
 * Formats have different names in different header files. Here is the mapping
 * table:
 *
 * android_pixel_format_t          videodev2.h           FOURCC in libyuv
 * -----------------------------------------------------------------------------
 * HAL_PIXEL_FORMAT_YV12         = V4L2_PIX_FMT_YVU420 = FOURCC_YV12
 * HAL_PIXEL_FORMAT_YCrCb_420_SP = V4L2_PIX_FMT_NV21   = FOURCC_NV21
 * HAL_PIXEL_FORMAT_BGRA_8888    = V4L2_PIX_FMT_BGR32  = FOURCC_ARGB
 * HAL_PIXEL_FORMAT_YCbCr_422_I  = V4L2_PIX_FMT_YUYV   = FOURCC_YUYV
 *                                                     = FOURCC_YUY2
 *                                 V4L2_PIX_FMT_YUV420 = FOURCC_I420
 *                                                     = FOURCC_YU12
 *                                 V4L2_PIX_FMT_MJPEG  = FOURCC_MJPG
 *
 * Camera device generates FOURCC_YUYV and FOURCC_MJPG.
 * Preview needs FOURCC_ARGB format.
 * Software video encoder needs FOURCC_YU12.
 * CTS requires FOURCC_YV12 and FOURCC_NV21 for applications.
 *
 * Android stride requirement:
 * YV12 horizontal stride should be a multiple of 16 pixels. See
 * android.graphics.ImageFormat.YV12.
 * The stride of ARGB, YU12, and NV21 are always equal to the width.
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

size_t ImageProcessor::GetConvertedSize(int fourcc,
                                        uint32_t width,
                                        uint32_t height) {
  if ((width % 2) || (height % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << width << " x " << height
                << ")";
    return 0;
  }

  switch (fourcc) {
    case V4L2_PIX_FMT_YVU420:  // YV12
      return Align16(width) * height + Align16(width / 2) * height;
    case V4L2_PIX_FMT_YUV420:  // YU12
    // Fall-through.
    case V4L2_PIX_FMT_NV21:  // NV21
      return width * height * 3 / 2;
    case V4L2_PIX_FMT_BGR32:
      return width * height * 4;
    default:
      LOGF(ERROR) << "Pixel format 0x" << std::hex << fourcc
                  << " is unsupported.";
      return 0;
  }
}

int ImageProcessor::Convert(const FrameBuffer& in_frame,
                            FrameBuffer* out_frame) {
  if ((in_frame.GetWidth() % 2) || (in_frame.GetHeight() % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << in_frame.GetWidth()
                << " x " << in_frame.GetHeight() << ")";
    return -EINVAL;
  }

  size_t data_size = GetConvertedSize(
      out_frame->GetFourcc(), in_frame.GetWidth(), in_frame.GetHeight());

  if (out_frame->SetDataSize(data_size)) {
    LOGF(ERROR) << "Set data size failed";
    return -EINVAL;
  }

  if (in_frame.GetFourcc() == V4L2_PIX_FMT_YUYV) {
    switch (out_frame->GetFourcc()) {
      case V4L2_PIX_FMT_YUV420:  // YU12
      {
        int res = libyuv::YUY2ToI420(
            in_frame.GetData(), in_frame.GetWidth() * 2, out_frame->GetData(),
            out_frame->GetWidth(),
            out_frame->GetData() +
                out_frame->GetWidth() * out_frame->GetHeight(),
            out_frame->GetWidth() / 2,
            out_frame->GetData() +
                out_frame->GetWidth() * out_frame->GetHeight() * 5 / 4,
            out_frame->GetWidth() / 2, in_frame.GetWidth(),
            in_frame.GetHeight());
        LOGF_IF(ERROR, res) << "YUY2ToI420() for YU12 returns " << res;
        return res ? -EINVAL : 0;
      }
      default:
        LOGF(ERROR) << "Destination pixel format "
                    << FormatToString(out_frame->GetFourcc())
                    << " is unsupported for YUYV source format.";
        return -EINVAL;
    }
  } else if (in_frame.GetFourcc() == V4L2_PIX_FMT_YUV420) {
    // V4L2_PIX_FMT_YVU420 is YV12. I420 is usually referred to YU12
    // (V4L2_PIX_FMT_YUV420), and YV12 is similar to YU12 except that U/V
    // planes are swapped.
    switch (out_frame->GetFourcc()) {
      case V4L2_PIX_FMT_YVU420:  // YV12
      {
        int ystride = Align16(in_frame.GetWidth());
        int uvstride = Align16(in_frame.GetWidth() / 2);
        int res = YU12ToYV12(in_frame.GetData(), out_frame->GetData(),
                             in_frame.GetWidth(), in_frame.GetHeight(), ystride,
                             uvstride);
        LOGF_IF(ERROR, res) << "YU12ToYV12() returns " << res;
        return res ? -EINVAL : 0;
      }
      case V4L2_PIX_FMT_YUV420:  // YU12
      {
        memcpy(out_frame->GetData(), in_frame.GetData(),
               in_frame.GetDataSize());
        return 0;
      }
      case V4L2_PIX_FMT_NV21:  // NV21
      {
        // TODO(henryhsu): Use libyuv::I420ToNV21.
        int res = YU12ToNV21(in_frame.GetData(), out_frame->GetData(),
                             in_frame.GetWidth(), in_frame.GetHeight());
        LOGF_IF(ERROR, res) << "YU12ToNV21() returns " << res;
        return res ? -EINVAL : 0;
      }
      case V4L2_PIX_FMT_BGR32: {
        int res = libyuv::I420ToARGB(
            in_frame.GetData(), in_frame.GetWidth(),
            in_frame.GetData() + in_frame.GetWidth() * in_frame.GetHeight(),
            in_frame.GetWidth() / 2,
            in_frame.GetData() +
                in_frame.GetWidth() * in_frame.GetHeight() * 5 / 4,
            in_frame.GetWidth() / 2, out_frame->GetData(),
            out_frame->GetWidth() * 4, in_frame.GetWidth(),
            in_frame.GetHeight());
        LOGF_IF(ERROR, res) << "I420ToARGB() returns " << res;
        return res ? -EINVAL : 0;
      }
      default:
        LOGF(ERROR) << "Destination pixel format "
                    << FormatToString(out_frame->GetFourcc())
                    << " is unsupported for YU12 source format.";
        return -EINVAL;
    }
  } else if (in_frame.GetFourcc() == V4L2_PIX_FMT_MJPEG) {
    switch (out_frame->GetFourcc()) {
      case V4L2_PIX_FMT_YUV420:  // YU12
      {
        int res = libyuv::MJPGToI420(
            in_frame.GetData(), in_frame.GetDataSize(), out_frame->GetData(),
            out_frame->GetWidth(),
            out_frame->GetData() +
                out_frame->GetWidth() * out_frame->GetHeight(),
            out_frame->GetWidth() / 2,
            out_frame->GetData() +
                out_frame->GetWidth() * out_frame->GetHeight() * 5 / 4,
            out_frame->GetWidth() / 2, in_frame.GetWidth(),
            in_frame.GetHeight(), out_frame->GetWidth(),
            out_frame->GetHeight());
        LOGF_IF(ERROR, res) << "MJPEGToI420() returns " << res;
        return res ? -EINVAL : 0;
      }
      default:
        LOGF(ERROR) << "Destination pixel format "
                    << FormatToString(out_frame->GetFourcc())
                    << " is unsupported for MJPEG source format.";
        return -EINVAL;
    }
  } else {
    LOGF(ERROR) << "Convert format doesn't support source format "
                << FormatToString(in_frame.GetFourcc());
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

}  // namespace arc
