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
                             const std::string& customization_id,
                             bool use_yaml) {
  base::FilePath smbios_file, vpd_file;
  CrosConfigIdentity identity;
  if (!identity.FakeIdentity(name, sku_id, customization_id, &smbios_file,
                             &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeIdentity() failed";
    return false;
  }
  return InitCommon(filepath, smbios_file, vpd_file, use_yaml);
}

bool CrosConfig::InitModel() {
  return cros_config_fdt_.InitModel();
}

bool CrosConfig::CompareResults(bool fdt_ok,
                                const std::string& fdt_val,
                                bool yaml_ok,
                                const std::string& yaml_val,
                                std::string* val_out) {
  if (fdt_ok != yaml_ok) {
    CROS_CONFIG_LOG(ERROR) << "Mismatch fdt_ok=" << fdt_ok
                           << ", yaml_ok=" << yaml_ok;
    return false;
  }
  if (fdt_val != yaml_val) {
    CROS_CONFIG_LOG(ERROR) << "Mismatch fdt_val='" << fdt_val << "', yaml_val='"
                           << yaml_val << "'";
    return false;
  }

  *val_out = fdt_val;
  return fdt_ok;
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out) {
  std::string fdt_val;
  bool fdt_ok = cros_config_fdt_.GetString(path, prop, &fdt_val);
  if (!use_yaml_) {
    *val_out = fdt_val;
    return fdt_ok;
  }
  std::string yaml_val;
  bool yaml_ok = cros_config_yaml_.GetString(path, prop, &yaml_val);
  return CompareResults(fdt_ok, fdt_val, yaml_ok, yaml_val, val_out);
}

bool CrosConfig::GetAbsPath(const std::string& path,
                            const std::string& prop,
                            std::string* val_out) {
  std::string fdt_val;
  bool fdt_ok = cros_config_fdt_.GetAbsPath(path, prop, &fdt_val);
  if (!use_yaml_) {
    *val_out = fdt_val;
    return fdt_ok;
  }
  std::string yaml_val;
  bool yaml_ok = cros_config_yaml_.GetAbsPath(path, prop, &yaml_val);
  return CompareResults(fdt_ok, fdt_val, yaml_ok, yaml_val, val_out);
}

bool CrosConfig::InitCommon(const base::FilePath& filepath,
                            const base::FilePath& mem_file,
                            const base::FilePath& vpd_file,
                            bool use_yaml) {
  if (!cros_config_fdt_.InitCommon(filepath, mem_file, vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "Failed to set up FDT "
                           << filepath.MaybeAsASCII();
    return false;
  }
  if (use_yaml) {
    base::FilePath yaml_filepath =
        filepath.RemoveExtension().AddExtension(".json");
    if (!cros_config_yaml_.InitCommon(yaml_filepath, mem_file, vpd_file)) {
      CROS_CONFIG_LOG(ERROR) << "Failed to set up yaml "
                            << yaml_filepath.MaybeAsASCII();
      return false;
    }
  }
  use_yaml_ = use_yaml;

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
