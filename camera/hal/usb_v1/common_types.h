/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_USB_V1_COMMON_TYPES_H_
#define CAMERA_HAL_USB_V1_COMMON_TYPES_H_

#include <string>
#include <vector>

namespace arc {

// The types in this file should match Android camera HAL.

struct DeviceInfo {
  std::string device_path;
  std::string usb_vid;  // USB vender id
  std::string usb_pid;  // USB product id
  uint32_t
      lens_facing;  // Direction the camera faces relative to device screen.
  // Clockwise angle through which the output image needs to be rotated to be
  // upright on the device screen in its native orientation.
  int32_t sensor_orientation;
  uint32_t frames_to_skip_after_streamon;
  float horizontal_view_angle_16_9;
  float horizontal_view_angle_4_3;
  std::vector<float> lens_info_available_focal_lengths;
  float lens_info_minimum_focus_distance;
  float lens_info_optimal_focus_distance;
  float vertical_view_angle_16_9;
  float vertical_view_angle_4_3;
  // The camera doesn't support 1280x960 resolution when the maximum resolution
  // of the camear is larger than 1080p.
  bool resolution_1280x960_unsupported;
  // The camera doesn't support 1600x1200 resolution.
  bool resolution_1600x1200_unsupported;
  // The camera doesn't support constant frame rate. That means HAL cannot set
  // V4L2_CID_EXPOSURE_AUTO_PRIORITY to 0 to have constant frame rate in low
  // light environment.
  bool constant_framerate_unsupported;
  uint32_t sensor_info_pixel_array_size_width;
  uint32_t sensor_info_pixel_array_size_height;
};

typedef std::vector<DeviceInfo> DeviceInfos;

struct SupportedFormat {
  uint32_t width;
  uint32_t height;
  uint32_t fourcc;
  // All the supported frame rates in fps with given width, height, and
  // pixelformat. This is not sorted. For example, suppose width, height, and
  // fourcc are 640x480 YUYV. If frameRates are 15.0 and 30.0, the camera
  // supports outputting  640X480 YUYV in 15fps or 30fps.
  std::vector<float> frameRates;
};

typedef std::vector<SupportedFormat> SupportedFormats;

}  // namespace arc

#endif  // CAMERA_HAL_USB_V1_COMMON_TYPES_H_
