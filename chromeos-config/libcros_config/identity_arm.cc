// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/identity_arm.h"

#include <string>
#include <regex.h>

#include <base/logging.h>
#include <base/files/file_util.h>

namespace brillo {

CrosConfigIdentityArm::CrosConfigIdentityArm() {}

CrosConfigIdentityArm::~CrosConfigIdentityArm() {}

bool CrosConfigIdentityArm::FakeDtCompatible(
    const std::string& device_name, base::FilePath* dt_compatible_file_out) {
  *dt_compatible_file_out = base::FilePath("dt_compatible");
  if (base::WriteFile(*dt_compatible_file_out, device_name.c_str(),
                      device_name.length()) != device_name.length()) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write device-tree compatible file";
    return false;
  }

  return true;
}

bool CrosConfigIdentityArm::ReadDtCompatible(
    const base::FilePath& dt_compatible_file) {
  if (!base::ReadFileToString(dt_compatible_file, &compatible_devices_)) {
    CROS_CONFIG_LOG(ERROR) << "Failed to read device-tree compatible file: "
                           << dt_compatible_file.MaybeAsASCII();
    return false;
  }
  CROS_CONFIG_LOG(INFO) << "Read device-tree compatible list: "
                        << compatible_devices_;
  return true;
}

bool CrosConfigIdentityArm::IsCompatible(const std::string& device_name) const {
  bool basic_match = compatible_devices_.find(device_name) != std::string::npos;
  bool regex_match = false;
  if (!basic_match) {
    regex_t regex;
    if (regcomp(&regex, device_name.c_str(),
                REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0) {
      regex_match =
          regexec(&regex, compatible_devices_.c_str(), 0, nullptr, 0) == 0;
      regfree(&regex);
    }
  }

  return basic_match || regex_match;
}

}  // namespace brillo
