// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

#include <map>
#include <set>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>
#include <chromeos/strings/string_utils.h>
#include <weave/enum_to_string.h>

namespace buffet {

namespace {

const char kErrorDomain[] = "buffet";
const char kFileReadError[] = "file_read_error";

bool StringToTimeDelta(const std::string& value, base::TimeDelta* delta) {
  uint64_t ms{0};
  if (!base::StringToUint64(value, &ms))
    return false;
  *delta = base::TimeDelta::FromMilliseconds(ms);
  return true;
}

bool LoadFile(const base::FilePath& file_path,
              std::string* data,
              chromeos::ErrorPtr* error) {
  if (!base::ReadFileToString(file_path, data)) {
    chromeos::errors::system::AddSystemError(error, FROM_HERE, errno);
    chromeos::Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kFileReadError,
                                 "Failed to read file '%s'",
                                 file_path.value().c_str());
    return false;
  }
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

BuffetConfig::BuffetConfig(const BuffetConfigPaths& paths) : paths_(paths) {}

void BuffetConfig::AddOnChangedCallback(const OnChangedCallback& callback) {
  on_changed_.push_back(callback);
}

bool BuffetConfig::LoadDefaults(weave::Settings* settings) {
  if (!base::PathExists(paths_.defaults))
    return true;  // Nothing to load.

  chromeos::KeyValueStore store;
  if (!store.Load(paths_.defaults))
    return false;
  return LoadDefaults(store, settings);
}

std::string BuffetConfig::LoadBaseCommandDefs() {
  // Load global standard GCD command dictionary.
  base::FilePath path{paths_.definitions.Append("gcd.json")};
  LOG(INFO) << "Loading standard commands from " << path.value();
  std::string result;
  CHECK(LoadFile(path, &result, nullptr));
  return result;
}

std::map<std::string, std::string> BuffetConfig::LoadCommandDefs() {
  std::map<std::string, std::string> result;
  auto load_packages = [&result](const base::FilePath& root,
                                 const base::FilePath::StringType& pattern) {
    base::FilePath dir{root.Append("commands")};
    VLOG(2) << "Looking for commands in " << dir.value();
    base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES,
                                    pattern);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      LOG(INFO) << "Loading command schema from " << path.value();
      std::string category = path.BaseName().RemoveExtension().value();
      CHECK(LoadFile(path, &result[category], nullptr));
    }
  };
  load_packages(paths_.definitions, FILE_PATH_LITERAL("*.json"));
  load_packages(paths_.test_definitions, FILE_PATH_LITERAL("*test.json"));
  return result;
}

std::string BuffetConfig::LoadBaseStateDefs() {
  // Load standard device state definition.
  base::FilePath path{paths_.definitions.Append("base_state.schema.json")};
  LOG(INFO) << "Loading standard state definition from " << path.value();
  std::string result;
  CHECK(LoadFile(path, &result, nullptr));
  return result;
}

std::string BuffetConfig::LoadBaseStateDefaults() {
  // Load standard device state defaults.
  base::FilePath path{paths_.definitions.Append("base_state.defaults.json")};
  LOG(INFO) << "Loading base state defaults from " << path.value();
  std::string result;
  CHECK(LoadFile(path, &result, nullptr));
  return result;
}

std::map<std::string, std::string> BuffetConfig::LoadStateDefs() {
  // Load component-specific device state definitions.
  base::FilePath dir{paths_.definitions.Append("states")};
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.schema.json"));
  std::map<std::string, std::string> result;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    LOG(INFO) << "Loading state definition from " << path.value();
    std::string category = path.BaseName().RemoveExtension().value();
    CHECK(LoadFile(path, &result[category], nullptr));
  }
  return result;
}

std::vector<std::string> BuffetConfig::LoadStateDefaults() {
  // Load component-specific device state defaults.
  base::FilePath dir{paths_.definitions.Append("states")};
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.defaults.json"));
  std::vector<std::string> result;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    LOG(INFO) << "Loading state defaults from " << path.value();
    std::string json;
    CHECK(LoadFile(path, &json, nullptr));
    result.push_back(json);
  }
  return result;
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
  base::ReadFileToString(paths_.settings, &json_string);
  return json_string;
}

void BuffetConfig::SaveSettings(const std::string& settings) {
  base::ImportantFileWriter::WriteFileAtomically(paths_.settings, settings);
}

void BuffetConfig::OnSettingsChanged(const weave::Settings& settings) {
  for (const auto& cb : on_changed_)
    cb.Run(settings);
}

}  // namespace buffet
