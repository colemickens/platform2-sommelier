// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/identity.h"

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>

namespace brillo {

CrosConfig::CrosConfig() {}

CrosConfig::~CrosConfig() {}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitForConfig(const base::FilePath& filepath) {
  return cros_config_fdt_.InitForConfig(filepath);
}

bool CrosConfig::InitForTest(const base::FilePath& filepath,
                             const std::string& name,
                             int sku_id,
                             const std::string& customization_id) {
  base::FilePath smbios_file, vpd_file;
  CrosConfigIdentity identity;
  if (!identity.FakeIdentity(name, sku_id, customization_id, &smbios_file,
                             &vpd_file)) {
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

bool CrosConfig::InitModel() {
  return cros_config_fdt_.InitModel();
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out) {
  return cros_config_fdt_.GetString(path, prop, val_out);
}

bool CrosConfig::GetAbsPath(const std::string& path,
                            const std::string& prop,
                            std::string* val_out) {
  return cros_config_fdt_.GetAbsPath(path, prop, val_out);
}

bool CrosConfig::InitCommon(const base::FilePath& filepath,
                            const base::FilePath& mem_file,
                            const base::FilePath& vpd_file) {
  if (!cros_config_fdt_.InitCommon(filepath, mem_file, vpd_file)) {
    return false;
  }
  inited_ = true;

  return true;
}

bool CrosConfig::GetString(ConfigNode base_node,
                           const std::string& path,
                           const std::string& prop,
                           std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  return cros_config_fdt_.GetString(base_node, path, prop, val_out,
                                    log_msgs_out);
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  return cros_config_fdt_.GetString(path, prop, val_out, log_msgs_out);
}

}  // namespace brillo
