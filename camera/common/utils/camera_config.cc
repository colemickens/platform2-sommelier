/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/utils/camera_config.h"

#include <iomanip>
#include <memory>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/values.h>

#include "cros-camera/common.h"

namespace cros {

CameraConfig::CameraConfig(const std::string& config_path_string)
    : config_(std::make_unique<base::DictionaryValue>()) {
  const base::FilePath config_path(config_path_string);

  if (!base::PathExists(config_path)) {
    return;
  }

  std::string content;
  if (!base::ReadFileToString(config_path, &content)) {
    LOGF(ERROR) << "Failed to read camera configuration file:"
                << config_path_string;
    return;
  }

  auto dict = base::DictionaryValue::From(base::JSONReader::Read(content));
  if (!dict) {
    LOGF(ERROR) << "Invalid JSON format of camera configuration file";
    return;
  }
  config_ = std::move(dict);
}

bool CameraConfig::HasKey(const std::string& key) const {
  return config_->HasKey(key);
}

bool CameraConfig::GetBoolean(const std::string& path,
                              bool default_value) const {
  bool value;
  return config_->GetBoolean(path, &value) ? value : default_value;
}

int CameraConfig::GetInteger(const std::string& path, int default_value) const {
  int value;
  return config_->GetInteger(path, &value) ? value : default_value;
}

}  // namespace cros
