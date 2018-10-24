// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/identity_arm.h"

#include <string>

#include <base/logging.h>
#include <base/files/file_util.h>

namespace brillo {

CrosConfigIdentityArm::CrosConfigIdentityArm() {}

CrosConfigIdentityArm::~CrosConfigIdentityArm() {}

bool CrosConfigIdentityArm::Fake(
    const std::string& device_name,
    int sku_id,
    base::FilePath* dt_compatible_file_out,
    base::FilePath* sku_id_file_out) {
  *dt_compatible_file_out = base::FilePath("dt_compatible");
  if (base::WriteFile(*dt_compatible_file_out, device_name.c_str(),
                      device_name.length()) != device_name.length()) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write device-tree compatible file";
    return false;
  }

  char *sku_id_char = reinterpret_cast<char*>(malloc(sizeof(sku_id)));
  *sku_id_file_out = base::FilePath("sku-id");

  std::memcpy(sku_id_char, &sku_id, sizeof(sku_id));
  if (base::WriteFile(*sku_id_file_out, sku_id_char,
                      sizeof(sku_id)) != sizeof(sku_id)) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write sku-id file";
    free(sku_id_char);
    return false;
  }

  free(sku_id_char);
  return true;
}

bool CrosConfigIdentityArm::ReadInfo(const base::FilePath& dt_compatible_file,
                                     const base::FilePath& sku_id_file) {
  if (!base::ReadFileToString(dt_compatible_file, &compatible_devices_)) {
    CROS_CONFIG_LOG(ERROR) << "Failed to read device-tree compatible file: "
                           << dt_compatible_file.MaybeAsASCII();
    return false;
  }

  char *sku_id_char = reinterpret_cast<char*>(malloc(sizeof(sku_id_)));
  if (base::ReadFile(sku_id_file, sku_id_char,
                     sizeof(sku_id_)) != sizeof(sku_id_)) {
    CROS_CONFIG_LOG(WARNING) << "Cannot read product_sku file ";
    free(sku_id_char);
    return false;
  }
  std::memcpy(&sku_id_, sku_id_char, sizeof(sku_id_));

  CROS_CONFIG_LOG(INFO) << "Read device-tree compatible list: "
                        << compatible_devices_
                        << ", sku_id: " << sku_id_;
  free(sku_id_char);
  return true;
}

bool CrosConfigIdentityArm::IsCompatible(const std::string& device_name) const {
  return compatible_devices_.find(device_name) != std::string::npos;
}

}  // namespace brillo
