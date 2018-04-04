// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/cros_config_fdt.h"
#include "chromeos-config/libcros_config/cros_config_json.h"
#include "chromeos-config/libcros_config/identity.h"

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>

namespace {
const char kSmbiosTablePath[] = "/run/cros_config/SMBIOS";
const char kCustomizationId[] = "/sys/firmware/vpd/ro/customization_id";
const char kConfigDtbPath[] = "/usr/share/chromeos-config/config.dtb";
}  // namespace

namespace brillo {

CrosConfig::CrosConfig() {
  cros_config_fdt_ = new CrosConfigFdt();
  cros_config_json_ = new CrosConfigJson();
}

CrosConfig::~CrosConfig() {
  delete cros_config_fdt_;
  delete cros_config_json_;
}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitForTest(const base::FilePath& filepath,
                             const std::string& name,
                             int sku_id,
                             const std::string& customization_id,
                             bool use_json) {
  base::FilePath smbios_file, vpd_file;
  CrosConfigIdentity identity;
  if (!identity.FakeIdentity(name, sku_id, customization_id, &smbios_file,
                             &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeIdentity() failed";
    return false;
  }
  return InitCommon(filepath, smbios_file, vpd_file, use_json);
}

bool CrosConfig::InitModel() {
  base::FilePath smbios_file(kSmbiosTablePath);
  base::FilePath vpd_file(kCustomizationId);
  return InitCommon(base::FilePath(kConfigDtbPath), smbios_file, vpd_file,
                    true);
}

bool CrosConfig::CompareResults(bool fdt_ok,
                                const std::string& fdt_val,
                                bool json_ok,
                                const std::string& json_val,
                                std::string* val_out) {
  // TODO(sjg): For now, continue even if json does not agree
  *val_out = fdt_val;
  if (fdt_ok != json_ok) {
    CROS_CONFIG_LOG(ERROR) << "Mismatch fdt_ok=" << fdt_ok
                           << ", json_ok=" << json_ok;
  } else if (fdt_val != json_val) {
    CROS_CONFIG_LOG(ERROR) << "Mismatch fdt_val='" << fdt_val << "', json_val='"
                           << json_val << "'";
  } else {
    CROS_CONFIG_LOG(INFO) << "fdt and json agree";
  }
  return fdt_ok;
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out) {
  std::string fdt_val;
  bool fdt_ok = cros_config_fdt_->GetString(path, prop, &fdt_val);
  if (!use_json_) {
    *val_out = fdt_val;
    return fdt_ok;
  }
  std::string json_val;
  bool json_ok = cros_config_json_->GetString(path, prop, &json_val);
  return CompareResults(fdt_ok, fdt_val, json_ok, json_val, val_out);
}

bool CrosConfig::GetAbsPath(const std::string& path,
                            const std::string& prop,
                            std::string* val_out) {
  std::string fdt_val;
  bool fdt_ok = cros_config_fdt_->GetAbsPath(path, prop, &fdt_val);
  if (!use_json_) {
    *val_out = fdt_val;
    return fdt_ok;
  }
  std::string json_val;
  bool json_ok = cros_config_json_->GetAbsPath(path, prop, &json_val);
  return CompareResults(fdt_ok, fdt_val, json_ok, json_val, val_out);
}

bool CrosConfig::InitCommon(const base::FilePath& filepath,
                            const base::FilePath& mem_file,
                            const base::FilePath& vpd_file,
                            bool use_json) {
  CROS_CONFIG_LOG(INFO) << ">>>>> starting";
  if (!cros_config_fdt_->InitCommon(filepath, mem_file, vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "Failed to set up FDT "
                           << filepath.MaybeAsASCII();
    return false;
  }
  if (use_json) {
    base::FilePath json_filepath =
        filepath.RemoveExtension().AddExtension(".json");
    if (!cros_config_json_->InitCommon(json_filepath, mem_file, vpd_file)) {
      CROS_CONFIG_LOG(WARNING) << "Failed to set up json "
                               << json_filepath.MaybeAsASCII();
      use_json = false;
    }
  }
  use_json_ = use_json;
  CROS_CONFIG_LOG(INFO) << ">>>>> init complete, use_json=" << use_json;

  return true;
}

bool CrosConfig::GetString(const ConfigNode& base_node,
                           const std::string& path,
                           const std::string& prop,
                           std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  return cros_config_fdt_->GetString(base_node, path, prop, val_out,
                                     log_msgs_out);
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  return cros_config_fdt_->GetString(path, prop, val_out, log_msgs_out);
}

}  // namespace brillo
