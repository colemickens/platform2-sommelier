// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"
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
const char kCustomizationId[] = "/sys/firmware/vpd/ro/customization_id";
const char kWhitelabelTag[] = "/sys/firmware/vpd/ro/whitelabel_tag";
const char kProductName[] = "/sys/devices/virtual/dmi/id/product_name";
const char kProductSku[] = "/sys/devices/virtual/dmi/id/product_sku";
const char kArmSkuId[] = "/proc/device-tree/firmware/coreboot/sku-id";
const char kDeviceTreeCompatiblePath[] = "/proc/device-tree/compatible";
const char kConfigJsonPath[] = "/usr/share/chromeos-config/config.json";
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

bool CrosConfig::Init(const int sku_id) {
  return InitModel(sku_id);
}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitForTestX86(const base::FilePath& filepath,
                                const std::string& name,
                                int sku_id,
                                const std::string& customization_id) {
  base::FilePath product_name_file, product_sku_file, vpd_file;
  CrosConfigIdentityX86 identity;
  if (!identity.FakeVpd(customization_id, &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeVpd() failed";
    return false;
  }
  if (!identity.Fake(name, sku_id, &product_name_file, &product_sku_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeVpd() failed";
    return false;
  }
  return InitCrosConfig(filepath) &&
         SelectConfigByIdentityX86(product_name_file, product_sku_file,
                                   vpd_file);
}

bool CrosConfig::InitForTestArm(const base::FilePath& filepath,
                                const std::string& dt_compatible_name,
                                int sku_id,
                                const std::string& customization_id) {
  base::FilePath dt_compatible_file, sku_id_file, vpd_file;
  CrosConfigIdentityArm identity;
  if (!identity.FakeVpd(customization_id, &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeVpd() failed";
    return false;
  }
  if (!identity.Fake(dt_compatible_name, sku_id, &dt_compatible_file,
                     &sku_id_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeDtCompatible() failed";
    return false;
  }
  return InitCrosConfig(filepath) &&
         SelectConfigByIdentityArm(dt_compatible_file,
                                   sku_id_file, vpd_file);
}

bool CrosConfig::InitModel() {
  return InitModel(kDefaultSkuId);
}

bool CrosConfig::InitModel(const int sku_id) {
  base::FilePath json_path(kConfigJsonPath);
  bool init_config = false;
  if (base::PathExists(json_path)) {
    init_config = InitCrosConfig(json_path);
  }
  if (!init_config) {
    return false;
  }
  const std::string system_arch = base::SysInfo::OperatingSystemArchitecture();
  base::FilePath vpd_file(kWhitelabelTag);
  if (!base::PathExists(vpd_file)) {
    vpd_file = base::FilePath(kCustomizationId);
  }
  if (system_arch == "x86_64" || system_arch == "x86") {
    base::FilePath product_name_file(kProductName);
    base::FilePath product_sku_file(kProductSku);
    return SelectConfigByIdentityX86(product_name_file, product_sku_file,
                                     vpd_file, sku_id);
  } else {
    base::FilePath dt_compatible_file(kDeviceTreeCompatiblePath);
    base::FilePath sku_id_file(kArmSkuId);
    return SelectConfigByIdentityArm(dt_compatible_file, sku_id_file,
                                     vpd_file, sku_id);
  }
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val_out) {
  if (!InitCheck()) {
    return false;
  }
  return cros_config_->GetString(path, prop, val_out);
}

bool CrosConfig::SelectConfigByIdentityX86(
    const base::FilePath& product_name_file,
    const base::FilePath& product_sku_file,
    const base::FilePath& vpd_file,
    const int sku_id) {
  CROS_CONFIG_LOG(INFO) << ">>>>> Starting to read X86 identity";
  CrosConfigIdentityX86 identity;
  if (!identity.ReadVpd(vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read VPD identity";
    return false;
  }
  if (!identity.ReadInfo(product_name_file, product_sku_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read SMBIOS identity";
    return false;
  }
  if (sku_id != kDefaultSkuId) {
    identity.SetSkuId(sku_id);
    CROS_CONFIG_LOG(INFO) << "Set sku_id to explicitly assigned value "
                          << sku_id;
  }
  if (!cros_config_->SelectConfigByIdentityX86(identity)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot find config for"
                           << " name: " << identity.GetName()
                           << " SKU ID: " << identity.GetSkuId()
                           << " VPD ID from " << vpd_file.MaybeAsASCII() << ": "
                           << identity.GetVpdId();
    return false;
  }

  CROS_CONFIG_LOG(INFO) << ">>>>> Completed initialization with x86 identity";

  return true;
}

bool CrosConfig::SelectConfigByIdentityArm(
    const base::FilePath& dt_compatible_file,
    const base::FilePath& sku_id_file,
    const base::FilePath& vpd_file,
    const int sku_id) {
  CROS_CONFIG_LOG(INFO) << ">>>>> Starting to read ARM identity";
  CrosConfigIdentityArm identity;
  if (!identity.ReadVpd(vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read VPD identity";
    return false;
  }
  if (!identity.ReadInfo(dt_compatible_file, sku_id_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read device-tree compatible and "
                           << "sku-id identities";
    return false;
  }
  if (sku_id != kDefaultSkuId) {
    identity.SetSkuId(sku_id);
    CROS_CONFIG_LOG(INFO) << "Set sku_id to explicitly assigned value "
                          << sku_id;
  }
  if (!cros_config_->SelectConfigByIdentityArm(identity)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot find config for "
                           << "device-tree compatible string: "
                           << identity.GetCompatibleDeviceString()
                           << " SKU ID: " << identity.GetSkuId()
                           << " VPD ID from " << vpd_file.MaybeAsASCII()
                           << ": " << identity.GetVpdId();
    return false;
  }

  CROS_CONFIG_LOG(INFO) << ">>>>> Completed initialization with ARM identity";

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

  cros_config_ = std::make_unique<CrosConfigJson>();

  cros_config_->ReadConfigFile(filepath);

  CROS_CONFIG_LOG(INFO) << ">>>>> config file successfully read";

  return true;
}

}  // namespace brillo
