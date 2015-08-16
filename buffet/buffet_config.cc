// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

#include <set>

#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/strings/string_utils.h>
#include <weave/enum_to_string.h>

namespace buffet {

namespace {

// bool IsValidAccessRole(const std::string& role) {
//  return role == "none" || role == "viewer" || role == "user";
//}
//
bool StringToTimeDelta(const std::string& value, base::TimeDelta* delta) {
  uint64_t ms{0};
  if (!base::StringToUint64(value, &ms))
    return false;
  *delta = base::TimeDelta::FromMilliseconds(ms);
  return true;
}

}  // namespace

namespace config_keys {

const char kClientId[] = "client_id";
const char kClientSecret[] = "client_secret";
const char kApiKey[] = "api_key";
const char kOAuthURL[] = "oauth_url";
const char kServiceURL[] = "service_url";
const char kName[] = "name";
const char kDescription[] = "description";
const char kLocation[] = "location";
const char kLocalAnonymousAccessRole[] = "local_anonymous_access_role";
const char kLocalDiscoveryEnabled[] = "local_discovery_enabled";
const char kLocalPairingEnabled[] = "local_pairing_enabled";
const char kOemName[] = "oem_name";
const char kModelName[] = "model_name";
const char kModelId[] = "model_id";
const char kPollingPeriodMs[] = "polling_period_ms";
const char kBackupPollingPeriodMs[] = "backup_polling_period_ms";
const char kWifiAutoSetupEnabled[] = "wifi_auto_setup_enabled";
const char kBleSetupEnabled[] = "ble_setup_enabled";
const char kEmbeddedCode[] = "embedded_code";
const char kPairingModes[] = "pairing_modes";

}  // namespace config_keys

BuffetConfig::BuffetConfig(const base::FilePath& defaults_path,
                           const base::FilePath& settings_path)
    : defaults_path_{defaults_path}, settings_path_{settings_path} {}

void BuffetConfig::AddOnChangedCallback(const OnChangedCallback& callback) {
  on_changed_.push_back(callback);
}

bool BuffetConfig::LoadDefaults(weave::Settings* settings) {
  if (!base::PathExists(defaults_path_))
    return true;  // Nothing to load.

  chromeos::KeyValueStore store;
  if (!store.Load(defaults_path_))
    return false;
  return LoadDefaults(store, settings);
}

bool BuffetConfig::LoadDefaults(const chromeos::KeyValueStore& store,
                                weave::Settings* settings) {
  store.GetString(config_keys::kClientId, &settings->client_id);
  store.GetString(config_keys::kClientSecret, &settings->client_secret);
  store.GetString(config_keys::kApiKey, &settings->api_key);
  store.GetString(config_keys::kOAuthURL, &settings->oauth_url);
  store.GetString(config_keys::kServiceURL, &settings->service_url);
  store.GetString(config_keys::kOemName, &settings->oem_name);
  store.GetString(config_keys::kModelName, &settings->model_name);
  store.GetString(config_keys::kModelId, &settings->model_id);

  base::FilePath lsb_release_path("/etc/lsb-release");
  chromeos::KeyValueStore lsb_release_store;
  if (lsb_release_store.Load(lsb_release_path) &&
      lsb_release_store.GetString("CHROMEOS_RELEASE_VERSION",
                                  &settings->firmware_version)) {
  } else {
    LOG(ERROR) << "Failed to get CHROMEOS_RELEASE_VERSION from "
               << lsb_release_path.value();
  }

  std::string polling_period_str;
  if (store.GetString(config_keys::kPollingPeriodMs, &polling_period_str) &&
      !StringToTimeDelta(polling_period_str, &settings->polling_period)) {
    return false;
  }

  if (store.GetString(config_keys::kBackupPollingPeriodMs,
                      &polling_period_str) &&
      !StringToTimeDelta(polling_period_str,
                         &settings->backup_polling_period)) {
    return false;
  }

  store.GetBoolean(config_keys::kWifiAutoSetupEnabled,
                   &settings->wifi_auto_setup_enabled);
  store.GetBoolean(config_keys::kBleSetupEnabled, &settings->ble_setup_enabled);
  store.GetString(config_keys::kEmbeddedCode, &settings->embedded_code);

  std::string modes_str;
  if (store.GetString(config_keys::kPairingModes, &modes_str)) {
    std::set<weave::PairingType> pairing_modes;
    for (const std::string& mode :
         chromeos::string_utils::Split(modes_str, ",", true, true)) {
      weave::PairingType pairing_mode;
      if (!StringToEnum(mode, &pairing_mode))
        return false;
      pairing_modes.insert(pairing_mode);
    }
    settings->pairing_modes = std::move(pairing_modes);
  }

  store.GetString(config_keys::kName, &settings->name);
  store.GetString(config_keys::kDescription, &settings->description);
  store.GetString(config_keys::kLocation, &settings->location);
  store.GetString(config_keys::kLocalAnonymousAccessRole,
                  &settings->local_anonymous_access_role);
  store.GetBoolean(config_keys::kLocalDiscoveryEnabled,
                   &settings->local_discovery_enabled);
  store.GetBoolean(config_keys::kLocalPairingEnabled,
                   &settings->local_pairing_enabled);
  return true;
}

std::string BuffetConfig::LoadSettings() {
  std::string json_string;
  base::ReadFileToString(settings_path_, &json_string);
  return json_string;
}

void BuffetConfig::SaveSettings(const std::string& settings) {
  base::ImportantFileWriter::WriteFileAtomically(settings_path_, settings);
}

void BuffetConfig::OnSettingsChanged(const weave::Settings& settings) {
  for (const auto& cb : on_changed_)
    cb.Run(settings);
}

}  // namespace buffet
