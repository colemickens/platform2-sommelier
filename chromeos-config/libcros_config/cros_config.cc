// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/cros_config_fdt.h"
#include "chromeos-config/libcros_config/cros_config_json.h"
#include "chromeos-config/libcros_config/identity_x86.h"

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/sys_info.h>

namespace {
const char kSmbiosTablePath[] = "/run/cros_config/SMBIOS";
const char kCustomizationId[] = "/sys/firmware/vpd/ro/customization_id";
const char kConfigDtbPath[] = "/usr/share/chromeos-config/config.dtb";
const char kConfigJsonPath[] = "/usr/share/chromeos-config/config.json";
const char kDtbExtension[] = ".dtb";
}  // namespace

namespace brillo {

CrosConfig::CrosConfig() {}

CrosConfig::~CrosConfig() {}

bool CrosConfig::InitCheck() const {
  if (!cros_config_) {
    CROS_CONFIG_LOG(ERROR)
        << "Init*() must be called before accessing configuration";
    return false;
  }
  return true;
}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitForTestX86(const base::FilePath& filepath,
                                const std::string& name,
                                int sku_id,
                                const std::string& customization_id) {
  base::FilePath smbios_file, vpd_file;
  CrosConfigIdentityX86 identity;
  if (!identity.FakeVpd(customization_id, &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeVpdIdentity() failed";
    return false;
  }
  if (!identity.FakeSmbios(name, sku_id, &smbios_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeSmbiosIdentity() failed";
    return false;
  }
  return InitCrosConfig(filepath) &&
         SelectConfigByIdentityX86(smbios_file, vpd_file);
}

bool CrosConfig::InitModel() {
  base::FilePath json_path(kConfigJsonPath);
  bool init_config = false;
  if (base::PathExists(json_path)) {
    init_config = InitCrosConfig(json_path);
  } else {
    base::FilePath dtb_path(kConfigDtbPath);
    init_config = InitCrosConfig(dtb_path);
  }
  const std::string system_arch = base::SysInfo::OperatingSystemArchitecture();
  if (system_arch == "x86_64" || system_arch == "x86") {
    base::FilePath smbios_file(kSmbiosTablePath);
    base::FilePath vpd_file(kCustomizationId);
    return init_config && SelectConfigByIdentityX86(smbios_file, vpd_file);
  } else {
    CROS_CONFIG_LOG(ERROR) << "Only x86 system architectures are supported.";
    return false;
  }
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  if (!InitCheck()) {
    return false;
  }
  return cros_config_->GetString(path, prop, val_out, log_msgs_out);
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out) {
  if (!InitCheck()) {
    return false;
  }
  return cros_config_->GetString(path, prop, val_out);
}

bool CrosConfig::GetAbsPath(const std::string& path,
                            const std::string& prop,
                            std::string* val_out) {
  if (!InitCheck()) {
    return false;
  }
  return cros_config_->GetAbsPath(path, prop, val_out);
}

bool CrosConfig::SelectConfigByIdentityX86(const base::FilePath& mem_file,
                                           const base::FilePath& vpd_file) {
  CROS_CONFIG_LOG(INFO) << ">>>>> Starting to read X86 identity";
  CrosConfigIdentityX86 identity;
  if (!identity.ReadVpd(vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read VPD identity";
    return false;
  }
  if (!identity.ReadSmbios(mem_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read SMBIOS identity";
    return false;
  }
  std::string name = identity.GetName();
  std::string customization_id = identity.GetCustomizationId();
  int sku_id = identity.GetSkuId();
  if (!cros_config_->SelectConfigByIdentityX86(identity)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot find config for name " << name
                           << " SKU ID " << sku_id << " Customization ID "
                           << customization_id;
    return false;
  }

  CROS_CONFIG_LOG(INFO) << ">>>>> Completed initialized with x86 identity";

  return true;
}

bool CrosConfig::InitCrosConfig(const base::FilePath& filepath) {
  CROS_CONFIG_LOG(INFO) << ">>>>> reading config file: path="
                        << filepath.MaybeAsASCII();
  // Many systems will not have a config database (yet), so just skip all the
  // setup without any errors if the config file doesn't exist.
  if (!base::PathExists(filepath)) {
    return false;
  }

  if (filepath.MatchesExtension(kDtbExtension)) {
    cros_config_ = std::make_unique<CrosConfigFdt>();
  } else {
    cros_config_ = std::make_unique<CrosConfigJson>();
  }

  cros_config_->ReadConfigFile(filepath);

  CROS_CONFIG_LOG(INFO) << ">>>>> config file successfully read";

  return true;
}

}  // namespace brillo
