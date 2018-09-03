/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_CROS_CAMERA_CONSTANTS_H_
#define INCLUDE_CROS_CAMERA_CONSTANTS_H_

namespace cros {

namespace constants {

const char kCrosCameraAlgoSocketPathString[] = "/run/camera/camera-algo.sock";
const char kCrosCameraSocketPathString[] = "/run/camera/camera3.sock";
const char kCrosCameraTestConfigPathString[] = "/run/camera/test_config.json";

// Options for |TestConfigParser|:
// boolean value used in test mode for forcing hardware jpeg encode/decode in
// USB HAL (won't fallback to SW encode/decode).
const char kCrosForceJpegHardwareEncodeOption[] = "force_jpeg_hw_enc";
const char kCrosForceJpegHardwareDecodeOption[] = "force_jpeg_hw_dec";

}  // namespace constants
}  // namespace cros
#endif  // INCLUDE_CROS_CAMERA_CONSTANTS_H_
