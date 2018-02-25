// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-config/libcros_config/fake_cros_config.h"

#include <base/logging.h>

namespace brillo {

FakeCrosConfig::FakeCrosConfig() {}

FakeCrosConfig::~FakeCrosConfig() {}

void FakeCrosConfig::SetString(const std::string& path,
                               const std::string& prop,
                               const std::string& val) {
  values_[PathProp{path, prop}] = val;
}

bool FakeCrosConfig::GetString(const std::string& path,
                               const std::string& prop,
                               std::string* val) {
  auto it = values_.find(PathProp{path, prop});
  if (it == values_.end()) {
    CROS_CONFIG_LOG(WARNING) << "Cannot get path " << path << " property "
                             << prop << ": <fake_error>";
    return false;
  }
  *val = it->second;

  return true;
}

void FakeCrosConfig::SetTargetDir(const std::string& prop,
                                  const std::string& dirname) {
  target_dirs_[prop] = dirname;
}

bool FakeCrosConfig::GetAbsPath(const std::string& path,
                                const std::string& prop,
                                std::string* val_out) {
  std::string val;
  if (!GetString(path, prop, &val)) {
    return false;
  }

  auto match = target_dirs_.find(prop);
  if (match == target_dirs_.end()) {
    CROS_CONFIG_LOG(ERROR) << "Absolute path requested at path " << path
                           << " property " << prop << " but none is available";
    return false;
  }
  *val_out = match->second + "/" + val;

  return true;
}

}  // namespace brillo
