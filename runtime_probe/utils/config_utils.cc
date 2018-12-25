// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/sys_info.h>
#include <chromeos-config/libcros_config/cros_config.h>
#include <vboot/crossystem.h>

#include "runtime_probe/utils/config_utils.h"

using base::DictionaryValue;

namespace {

const char kCrosConfigModelNamePath[] = "/";
const char kCrosConfigModelNameKey[] = "name";
const char kRuntimeProbeConfigDir[] = "/etc/runtime_probe";
const char kRuntimeProbeConfigName[] = "probe_config.json";

void GetModelName(std::string* model_name) {
  auto cros_config = std::make_unique<brillo::CrosConfig>();

  if (cros_config->InitModel() &&
      cros_config->GetString(kCrosConfigModelNamePath, kCrosConfigModelNameKey,
                             model_name))
    return;

  // Fallback to sys_info.
  *model_name = base::SysInfo::GetLsbReleaseBoard();
}

std::string GetPathOfRootfsProbeConfig() {
  std::string model_name;
  const auto default_config =
      base::FilePath{kRuntimeProbeConfigDir}.Append(kRuntimeProbeConfigName);

  GetModelName(&model_name);
  auto config_path = base::FilePath{kRuntimeProbeConfigDir}
                         .Append(model_name)
                         .Append(kRuntimeProbeConfigName);
  if (base::PathExists(config_path))
    return config_path.value();

  VLOG(1) << "Model specific probe config " << config_path.value()
          << " doesn't exist";

  return default_config.value();
}

}  // namespace

namespace runtime_probe {

std::unique_ptr<DictionaryValue> ParseProbeConfig(
    const std::string& config_file_path) {
  std::string config_json;
  if (!base::ReadFileToString(base::FilePath(config_file_path), &config_json)) {
    LOG(ERROR) << "Config file doesn't exist. "
               << "Input config file path is: " << config_file_path;
    return nullptr;
  }
  std::unique_ptr<DictionaryValue> dict_val =
      DictionaryValue::From(base::JSONReader::Read(config_json));
  if (dict_val == nullptr) {
    LOG(ERROR) << "Failed to parse ProbeConfig from : [" << config_file_path
               << "]\nInput JSON string is:\n"
               << config_json;
  }
  return dict_val;
}

bool GetProbeConfigPath(std::string* probe_config_path,
                        const std::string& probe_config_path_from_cli) {
  // Caller not assigned. Using default one in rootfs.
  if (probe_config_path_from_cli.empty()) {
    VLOG(1) << "No config_file_path specified, picking default config.";
    *probe_config_path = GetPathOfRootfsProbeConfig();
    VLOG(1) << "Selected config file: " << *probe_config_path;
    return true;
  }

  // Caller assigned, check permission.
  if (VbGetSystemPropertyInt("cros_debug") != 1) {
    LOG(ERROR) << "Arbitrary ProbeConfig is only allowed with cros_debug=1";
    return false;
  }

  *probe_config_path = probe_config_path_from_cli;
  return true;
}

}  // namespace runtime_probe
