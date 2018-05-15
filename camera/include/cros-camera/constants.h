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
const char kCrosCameraTestModePathString[] = "/run/camera/enable_test";

}  // namespace constants
}  // namespace cros
#endif  // INCLUDE_CROS_CAMERA_CONSTANTS_H_
