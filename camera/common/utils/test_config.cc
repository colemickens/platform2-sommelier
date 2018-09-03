/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/utils/test_config.h"

#include <iomanip>
#include <memory>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/values.h>

#include "cros-camera/common.h"

namespace cros {

TestConfig::TestConfig() : config_(std::make_unique<base::DictionaryValue>()) {
  const base::FilePath kCrosCameraTestConfigPath(
      constants::kCrosCameraTestConfigPathString);
  if (!base::PathExists(kCrosCameraTestConfigPath)) {
    return;
  }

  std::string content;
  if (!base::ReadFileToString(kCrosCameraTestConfigPath, &content)) {
    LOGF(ERROR) << "Failed to read test configuration file";
    return;
  }

  auto dict = base::DictionaryValue::From(base::JSONReader::Read(content));
  if (!dict) {
    LOGF(ERROR) << "Invalid JSON format of test configuration file";
    return;
  }
  config_ = std::move(dict);
}

bool TestConfig::GetBoolean(const std::string& path, bool default_value) const {
  bool value;
  return config_->GetBoolean(path, &value) ? value : default_value;
}

}  // namespace cros
