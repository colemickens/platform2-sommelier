// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/string_split.h>

namespace {
const char kSmbiosTablePath[] = "/run/cros_config/SMBIOS";
const char kCustomizationId[] = "/sys/firmware/vpd/ro/customization_id";
}  // namespace

namespace brillo {

CrosConfig::CrosConfig() {}

CrosConfig::~CrosConfig() {}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitForConfig(const base::FilePath& filepath) {
  base::FilePath smbios_file(kSmbiosTablePath);
  base::FilePath vpd_file(kCustomizationId);
  return InitCommon(filepath, smbios_file, vpd_file);
}

bool CrosConfig::InitForTest(const base::FilePath& filepath,
                             const std::string& name, int sku_id,
                             const std::string& customization_id) {
  base::FilePath smbios_file, vpd_file;
  if (!FakeIdentity(name, sku_id, customization_id, &smbios_file, &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeIdentity() failed";
    return false;
  }
  return InitCommon(filepath, smbios_file, vpd_file);
}

bool CrosConfig::InitCheck() const {
  if (!inited_) {
    CROS_CONFIG_LOG(ERROR)
        << "Init*() must be called before accessing configuration";
    return false;
  }
  return true;
}

}  // namespace brillo
