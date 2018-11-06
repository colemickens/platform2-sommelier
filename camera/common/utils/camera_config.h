/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_UTILS_CAMERA_CONFIG_H_
#define COMMON_UTILS_CAMERA_CONFIG_H_

#include <memory>
#include <string>

#include <base/values.h>

#include <cros-camera/constants.h>

namespace cros {

// Read config from camera configure file.
// Reference for all options from: include/cros-camera/constants.h
class CameraConfig {
 public:
  explicit CameraConfig(const std::string& config_path_string);
  CameraConfig(const CameraConfig&) = delete;
  CameraConfig& operator=(const CameraConfig&) = delete;

  // Return value of |path| in config file. In case that path is not present in
  // test config or any error occurred, return default_value instead.
  bool GetBoolean(const std::string& path, bool default_value) const;

  // Return value of |path| in config file. In case that path is not present in
  // test config or any error occurred, return default_value instead.
  int GetInteger(const std::string& path, int default_value) const;

 private:
  std::unique_ptr<base::DictionaryValue> config_;
};

}  // namespace cros

#endif  // COMMON_UTILS_CAMERA_CONFIG_H_
