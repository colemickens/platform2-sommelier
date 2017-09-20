// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// resolution detectors

#include <linux/videodev2.h>
#ifdef HAS_VAAPI
#include <va/va.h>
#endif

#include "label_detect.h"

static const char kVideoDevicePattern[] = "/dev/video*";
static const char kDRMDevicePattern[] = "/dev/dri/renderD*";
static const int32_t width_4k = 3840;
static const int32_t height_4k = 2160;

/* Helper function for is_4k_device_h264().
 * Determines if a VAAPI device associated with given |fd| supports
 * H264 decoding and its maximum resolution is larger than 3840x2160.
 */
static bool is_vaapi_4k_device_h264(int fd) {
#ifdef HAS_VAAPI
#if VA_CHECK_VERSION(0, 35, 0)
  VAProfile va_profiles[] = {
    VAProfileH264Baseline,
    VAProfileH264Main,
    VAProfileH264High,
    VAProfileH264ConstrainedBaseline,
    VAProfileNone
  };
  int32_t resolution_width = 0;
  int32_t resolution_height = 0;
  if (is_vaapi_support_formats(
      fd, va_profiles, VAEntrypointVLD, VA_RT_FORMAT_YUV420)) {
    if (get_vaapi_max_resolution(fd, va_profiles, VAEntrypointVLD,
                                 &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
#endif
#endif
  return false;
}

/* Helper function for is_4k_device_vp8().
 * Determines if a VAAPI device associated with given |fd| supports
 * VP8 decoding and its maximum resolution is larger than 3840x2160.
 */
static bool is_vaapi_4k_device_vp8(int fd) {
#ifdef HAS_VAAPI
#if VA_CHECK_VERSION(0, 35, 0)
  VAProfile va_profiles[] = {
    VAProfileVP8Version0_3,
    VAProfileNone
  };
  int32_t resolution_width = 0;
  int32_t resolution_height = 0;
  if (is_vaapi_support_formats(
      fd, va_profiles, VAEntrypointVLD, VA_RT_FORMAT_YUV420)) {
    if (get_vaapi_max_resolution(fd, va_profiles, VAEntrypointVLD,
                                 &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
#endif
#endif
  return false;
}


/* Helper function for is_4k_device_vp9().
 * Determines if a VAAPI device associated with given |fd| supports
 * VP9 decoding and its maximum resolution is larger than 3840x2160.
 */
static bool is_vaapi_4k_device_vp9(int fd) {
#ifdef HAS_VAAPI
#if VA_CHECK_VERSION(0, 35, 0)
  VAProfile va_profiles[] = {
    VAProfileVP9Profile0,
    VAProfileNone
  };
  int32_t resolution_width;
  int32_t resolution_height;
  if (is_vaapi_support_formats(
      fd, va_profiles, VAEntrypointVLD, VA_RT_FORMAT_YUV420)) {
    if (get_vaapi_max_resolution(fd, va_profiles, VAEntrypointVLD,
                                 &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
#endif
#endif
  return false;
}

/* Helper function for is_4k_device_h264().
 * A V4L2 device supports 4k resolution H264 decoding, if it supports H264
 * decoding and the maximum resolution is larger than 3840x2160.
 */
static bool is_v4l2_4k_device_h264(int fd) {
  int32_t resolution_width;
  int32_t resolution_height;
  if (!is_hw_video_acc_device(fd)) {
    return false;
  }
  if (is_v4l2_support_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                             V4L2_PIX_FMT_H264)) {
    if (get_v4l2_max_resolution(fd, V4L2_PIX_FMT_H264,
                                &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
  if (is_v4l2_support_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                             V4L2_PIX_FMT_H264_SLICE)) {
    if (get_v4l2_max_resolution(fd, V4L2_PIX_FMT_H264_SLICE,
                                &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
  return false;
}

/* Helper function for is_4k_device_vp8().
 * A V4L2 device supports 4k resolution VP8 decoding, if it supports VP8
 * decoding and the maximum resolution is larger than 3840x2160.
 */
static bool is_v4l2_4k_device_vp8(int fd) {
  int32_t resolution_width;
  int32_t resolution_height;
  if (!is_hw_video_acc_device(fd)) {
    return false;
  }
  if (is_v4l2_support_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                             V4L2_PIX_FMT_VP8)) {
    if (get_v4l2_max_resolution(fd, V4L2_PIX_FMT_VP8,
                                &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
  if (is_v4l2_support_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                             V4L2_PIX_FMT_VP8_FRAME)) {
    if (get_v4l2_max_resolution(fd, V4L2_PIX_FMT_VP8_FRAME,
                                &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
  return false;
}


/* Helper function for is_4k_device_vp9().
 * A V4L2 device supports 4k resolution VP9 decoding, if it supports VP9
 * decoding and the maximum resolution is larger than 3840x2160.
 */
static bool is_v4l2_4k_device_vp9(int fd) {
  int32_t resolution_width;
  int32_t resolution_height;
  if (!is_hw_video_acc_device(fd)) {
    return false;
  }
  if (is_v4l2_support_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                             V4L2_PIX_FMT_VP9)) {
    if (get_v4l2_max_resolution(fd, V4L2_PIX_FMT_VP9,
                                &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
  if (is_v4l2_support_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                                    V4L2_PIX_FMT_VP9_FRAME)) {
    if (get_v4l2_max_resolution(fd, V4L2_PIX_FMT_VP9_FRAME,
                                &resolution_width, &resolution_height)) {
      return resolution_width >= width_4k && resolution_height >= height_4k;
    }
  }
  return false;
}

/* Return true, if either the VAAPI device supports 4k resolution H264 decoding,
 * has decoding entry point, and input YUV420 formats. Or there is a
 * /dev/video* device supporting 4k resolution H264 decoding.
 */
bool is_4k_device_h264(void) {
  return (is_any_device(kDRMDevicePattern, is_vaapi_4k_device_h264) ||
          is_any_device(kVideoDevicePattern, is_v4l2_4k_device_h264));
}

/* Return true, if either the VAAPI device supports 4k resolution VP8 decoding,
 * has decoding entry point, and input YUV420 formats. Or there is a
 * /dev/video* device supporting 4k resolution VP8 decoding.
 */
bool is_4k_device_vp8(void) {
  return (is_any_device(kDRMDevicePattern, is_vaapi_4k_device_vp8) ||
          is_any_device(kVideoDevicePattern, is_v4l2_4k_device_vp8));
}

/* Return true, if either the VAAPI device supports 4k resolution VP9 decoding,
 * has decoding entry point, and input YUV420 formats. Or there is a
 * /dev/video* device supporting 4k resolution VP9 decoding.
 */
bool is_4k_device_vp9(void) {
  return (is_any_device(kDRMDevicePattern, is_vaapi_4k_device_vp9) ||
          is_any_device(kVideoDevicePattern, is_v4l2_4k_device_vp9));
}

/* Determines "4k_video" label. Returns true if and only if a device
 * supports 4k resolution decoding for all its supported codec.
 */
bool detect_4k_video(void) {
  bool video_acc_h264 = detect_video_acc_h264();
  bool video_acc_vp8 = detect_video_acc_vp8();
  bool video_acc_vp9 = detect_video_acc_vp9();
  if (!video_acc_h264 && !video_acc_vp8 && !video_acc_vp9) {
    // If a device doesn't support any type of codec,
    // it shouldn't have '4k_device' label.
    return false;
  }

  if (video_acc_h264 && !is_4k_device_h264())
    return false;

  if (video_acc_vp8 && !is_4k_device_vp8())
    return false;

  if (video_acc_vp9 && !is_4k_device_vp9())
    return false;

  return true;
}
