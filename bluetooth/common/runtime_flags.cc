// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/runtime_flags.h"

#include <map>
#include <memory>
#include <utility>

#include <base/logging.h>
#include <chromeos-config/libcros_config/cros_config.h>

namespace bluetooth {

namespace {

const std::map<std::string, bool> kDefaultUseFlags = {
    {"enable-suspend-management", USE_BLUETOOTH_SUSPEND_MANAGEMENT},
};

bool Truthy(const std::string& value) {
  return (value == "1" || value == "true" || value == "True");
}

};  // namespace

void RuntimeFlags::Init() {
  use_flags_ = &kDefaultUseFlags;
  cros_config_ = std::make_unique<brillo::CrosConfig>();

  if (!cros_config_->Init()) {
    // If cros_config_->Init() isn't working, it's probably developer error.
    // Set CROS_CONFIG_DEBUG=1 and re-run binary to see more detailed failure
    // reasons.
    LOG(ERROR) << "Failed to initialize cros config.";
    return;
  }

  init_ = true;
}

bool RuntimeFlags::Get(const std::string& key) {
  if (!init_)
    return false;

  std::string config;
  bool result = cros_config_->GetString(kBluetoothPath, key, &config);

  if (result)
    return Truthy(config);

  if (use_flags_) {
    const auto found = use_flags_->find(key);
    if (found != use_flags_->end())
      return found->second;
  }

  return false;
}

bool RuntimeFlags::GetContent(const std::string& key, std::string* out) {
  if (!init_)
    return false;

  return cros_config_->GetString(kBluetoothPath, key, out);
}

}  // namespace bluetooth
