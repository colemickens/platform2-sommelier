// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"

#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include "chromeos-config/libcros_config/cros_config_json.h"
#include "chromeos-config/libcros_config/identity.h"

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
  base::FilePath vpd_file(kWhitelabelTag);
  if (!base::PathExists(vpd_file)) {
    vpd_file = base::FilePath(kCustomizationId);
  }

  base::FilePath product_name_file;
  base::FilePath product_sku_file;
  SystemArchitecture arch = CrosConfigIdentity::CurrentSystemArchitecture();
  if (arch == SystemArchitecture::kX86) {
    product_name_file = base::FilePath(kProductName);
    product_sku_file = base::FilePath(kProductSku);
  } else if (arch == SystemArchitecture::kArm) {
    product_name_file = base::FilePath(kDeviceTreeCompatiblePath);
    product_sku_file = base::FilePath(kArmSkuId);
  } else {
    CROS_CONFIG_LOG(ERROR) << "System architecture is unknown";
    return false;
  }

  base::FilePath json_path(kConfigJsonPath);
  return InitInternal(sku_id, json_path, arch, product_name_file,
                      product_sku_file, vpd_file);
}

// TODO(jrosenth): delete this alias once nobody is calling InitModel
bool CrosConfig::InitModel() {
  return Init();
}

bool CrosConfig::InitForTest(const int sku_id,
                             const base::FilePath& json_path,
                             const SystemArchitecture arch,
                             const std::string& name,
                             const std::string& customization_id) {
  auto identity = CrosConfigIdentity::FromArchitecture(arch);
  if (!identity) {
    CROS_CONFIG_LOG(ERROR) << "Provided architecture is unknown";
    return false;
  }
  base::FilePath product_name_file, product_sku_file, vpd_file;
  if (!identity->FakeVpdFileForTesting(customization_id, &vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeVpdFileForTesting() failed";
    return false;
  }
  if (!identity->FakeProductFilesForTesting(name, sku_id, &product_name_file,
                                            &product_sku_file)) {
    CROS_CONFIG_LOG(ERROR) << "FakeProductFilesForTesting() failed";
    return false;
  }
  return InitInternal(sku_id, json_path, arch, product_name_file,
                      product_sku_file, vpd_file);
}

bool CrosConfig::InitInternal(const int sku_id,
                              const base::FilePath& json_path,
                              const SystemArchitecture arch,
                              const base::FilePath& product_name_file,
                              const base::FilePath& product_sku_file,
                              const base::FilePath& vpd_file) {
  if (!base::PathExists(json_path)) {
    // TODO(crbug.com/991653): Fallback to mosys platform when running
    // on non-unibuild platforms
    //
    // Implemented in child CL (crrev.com/c/1761410)
    return false;
  }

  auto cros_config_json = std::make_unique<CrosConfigJson>();
  CROS_CONFIG_LOG(INFO) << ">>>>> reading config file: path="
                        << json_path.MaybeAsASCII();
  if (!cros_config_json->ReadConfigFile(json_path))
    return false;
  CROS_CONFIG_LOG(INFO) << ">>>>> config file successfully read";

  CROS_CONFIG_LOG(INFO) << ">>>>> Starting to read identity";
  auto identity = CrosConfigIdentity::FromArchitecture(arch);
  if (!identity->ReadVpd(vpd_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read VPD identity";
    return false;
  }
  if (!identity->ReadInfo(product_name_file, product_sku_file)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read SMBIOS or dt-compatible info";
    return false;
  }
  if (sku_id != kDefaultSkuId) {
    identity->SetSkuId(sku_id);
    CROS_CONFIG_LOG(INFO) << "Set sku_id to explicitly assigned value "
                          << sku_id;
  }
  if (!cros_config_json->SelectConfigByIdentity(*identity)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot find config for "
                           << identity->DebugString() << " (VPD ID from "
                           << vpd_file.MaybeAsASCII() << ")";
    return false;
  }
  CROS_CONFIG_LOG(INFO) << ">>>>> Completed initialization";

  // Downgrade CrosConfigJson to CrosConfigInterface now that
  // initialization has finished
  cros_config_ = std::move(cros_config_json);
  return true;
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& property,
                           std::string* val_out) {
  if (!InitCheck()) {
    return false;
  }
  return cros_config_->GetString(path, property, val_out);
}

}  // namespace brillo
