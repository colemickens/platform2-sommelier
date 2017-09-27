// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-config/libcros_config/fake_cros_config.h"

#include <base/logging.h>

namespace brillo {

FakeCrosConfig::FakeCrosConfig() {}

FakeCrosConfig::~FakeCrosConfig() {}

void FakeCrosConfig::SetString(const std::string& path, const std::string& prop,
                               const std::string& val) {
  values_[PathProp{path, prop}] = val;
}

void FakeCrosConfig::SetFirmwareUrisList(
    const std::vector<std::string>& values) {
  firmware_uris_ = values;
}

std::vector<std::string> FakeCrosConfig::GetFirmwareUris() const {
  return firmware_uris_;
}

bool FakeCrosConfig::GetString(const std::string& path, const std::string& prop,
                               std::string* val) {
  // TODO(sjg): Handle non-root nodes. For now this is disabled since the
  // real implmenetation (CrosConfig) does not support it and we want to keep
  // the Fake in sync. We can delete this code block once CrosConfig supports
  // non-root notes.
  if (path != "/") {
    LOG(ERROR) << "Cannot access non-root node " << path;
    return false;
  }

  auto it = values_.find(PathProp{path, prop});
  if (it == values_.end()) {
    LOG(WARNING) << "Cannot get path " << path << " property " << prop
                 << ": <fake_error>";
    return false;
  }
  *val = it->second;

  return true;
}

std::vector<std::string> FakeCrosConfig::GetModelNames() const {
  return std::vector<std::string>{"reef", "pyro"};
}

}  // namespace brillo
