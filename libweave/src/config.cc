// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/config.h"

#include <set>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <weave/enum_to_string.h>

#include "libweave/src/storage_impls.h"
#include "libweave/src/storage_interface.h"
#include "libweave/src/string_utils.h"

namespace {

bool IsValidAccessRole(const std::string& role) {
  return role == "none" || role == "viewer" || role == "user";
}

bool StringToTimeDelta(const std::string& value, base::TimeDelta* delta) {
  uint64_t ms{0};
  if (!base::StringToUint64(value, &ms))
    return false;
  *delta = base::TimeDelta::FromMilliseconds(ms);
  return true;
}

}  // namespace

namespace weave {

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
const char kRefreshToken[] = "refresh_token";
const char kDeviceId[] = "device_id";
const char kRobotAccount[] = "robot_account";
const char kWifiAutoSetupEnabled[] = "wifi_auto_setup_enabled";
const char kBleSetupEnabled[] = "ble_setup_enabled";
const char kEmbeddedCode[] = "embedded_code";
const char kPairingModes[] = "pairing_modes";
const char kLastConfiguredSsid[] = "last_configured_ssid";

}  // namespace config_keys

Settings Config::CreateDefaultSettings() {
  Settings result;
  result.client_id = "58855907228.apps.googleusercontent.com";
  result.client_secret = "eHSAREAHrIqPsHBxCE9zPPBi";
  result.api_key = "AIzaSyDSq46gG-AxUnC3zoqD9COIPrjolFsMfMA";
  result.oauth_url = "https://accounts.google.com/o/oauth2/";
  result.service_url = "https://www.googleapis.com/clouddevices/v1/";
  result.name = "Developer device";
  result.local_anonymous_access_role = "viewer";
  result.local_discovery_enabled = true;
  result.local_pairing_enabled = true;
  result.oem_name = "Chromium";
  result.model_name = "Brillo";
  result.model_id = "AAAAA";
  result.polling_period = base::TimeDelta::FromSeconds(7);
  result.backup_polling_period = base::TimeDelta::FromMinutes(30);
  result.wifi_auto_setup_enabled = true;
  result.ble_setup_enabled = false;
  result.pairing_modes.emplace(PairingType::kPinCode);
  return result;
}

Config::Config(std::unique_ptr<StorageInterface> storage)
    : storage_{std::move(storage)} {}

Config::Config(const base::FilePath& state_path)
    : Config{std::unique_ptr<StorageInterface>{new FileStorage{state_path}}} {}

void Config::AddOnChangedCallback(const OnChangedCallback& callback) {
  on_changed_.push_back(callback);
  // Force to read current state.
  callback.Run(settings_);
}

const Settings& Config::GetSettings() const {
  return settings_;
}

void Config::Load(const base::FilePath& config_path) {
  chromeos::KeyValueStore store;
  if (base::PathExists(config_path)) {
    CHECK(store.Load(config_path)) << "Unable to read or parse config file at"
                                   << config_path.value();
  }
  Load(store);
}

void Config::Load(const chromeos::KeyValueStore& store) {
  Transaction change{this};
  change.save_ = false;

  store.GetString(config_keys::kClientId, &settings_.client_id);
  CHECK(!settings_.client_id.empty());

  store.GetString(config_keys::kClientSecret, &settings_.client_secret);
  CHECK(!settings_.client_secret.empty());

  store.GetString(config_keys::kApiKey, &settings_.api_key);
  CHECK(!settings_.api_key.empty());

  store.GetString(config_keys::kOAuthURL, &settings_.oauth_url);
  CHECK(!settings_.oauth_url.empty());

  store.GetString(config_keys::kServiceURL, &settings_.service_url);
  CHECK(!settings_.service_url.empty());

  store.GetString(config_keys::kOemName, &settings_.oem_name);
  CHECK(!settings_.oem_name.empty());

  store.GetString(config_keys::kModelName, &settings_.model_name);
  CHECK(!settings_.model_name.empty());

  store.GetString(config_keys::kModelId, &settings_.model_id);
  CHECK(!settings_.model_id.empty());

  std::string polling_period_str;
  if (store.GetString(config_keys::kPollingPeriodMs, &polling_period_str))
    CHECK(StringToTimeDelta(polling_period_str, &settings_.polling_period));

  if (store.GetString(config_keys::kBackupPollingPeriodMs, &polling_period_str))
    CHECK(StringToTimeDelta(polling_period_str,
                            &settings_.backup_polling_period));

  store.GetBoolean(config_keys::kWifiAutoSetupEnabled,
                   &settings_.wifi_auto_setup_enabled);

  store.GetBoolean(config_keys::kBleSetupEnabled, &settings_.ble_setup_enabled);

  store.GetString(config_keys::kEmbeddedCode, &settings_.embedded_code);

  std::string modes_str;
  if (store.GetString(config_keys::kPairingModes, &modes_str)) {
    std::set<PairingType> pairing_modes;
    for (const std::string& mode : Split(modes_str, ",", true, true)) {
      PairingType pairing_mode;
      CHECK(StringToEnum(mode, &pairing_mode));
      pairing_modes.insert(pairing_mode);
    }
    settings_.pairing_modes = std::move(pairing_modes);
  }

  // Empty name set by user or server is allowed, still we expect some
  // meaningfull config value.
  store.GetString(config_keys::kName, &settings_.name);
  CHECK(!settings_.name.empty());

  store.GetString(config_keys::kDescription, &settings_.description);
  store.GetString(config_keys::kLocation, &settings_.location);

  store.GetString(config_keys::kLocalAnonymousAccessRole,
                  &settings_.local_anonymous_access_role);
  CHECK(IsValidAccessRole(settings_.local_anonymous_access_role))
      << "Invalid role: " << settings_.local_anonymous_access_role;

  store.GetBoolean(config_keys::kLocalDiscoveryEnabled,
                   &settings_.local_discovery_enabled);
  store.GetBoolean(config_keys::kLocalPairingEnabled,
                   &settings_.local_pairing_enabled);

  change.LoadState();
}

void Config::Transaction::LoadState() {
  if (!config_->storage_)
    return;
  auto value = config_->storage_->Load();
  const base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict))
    return;

  std::string tmp;
  bool tmp_bool{false};

  if (dict->GetString(config_keys::kClientId, &tmp))
    set_client_id(tmp);

  if (dict->GetString(config_keys::kClientSecret, &tmp))
    set_client_secret(tmp);

  if (dict->GetString(config_keys::kApiKey, &tmp))
    set_api_key(tmp);

  if (dict->GetString(config_keys::kOAuthURL, &tmp))
    set_oauth_url(tmp);

  if (dict->GetString(config_keys::kServiceURL, &tmp))
    set_service_url(tmp);

  if (dict->GetString(config_keys::kName, &tmp))
    set_name(tmp);

  if (dict->GetString(config_keys::kDescription, &tmp))
    set_description(tmp);

  if (dict->GetString(config_keys::kLocation, &tmp))
    set_location(tmp);

  if (dict->GetString(config_keys::kLocalAnonymousAccessRole, &tmp))
    set_local_anonymous_access_role(tmp);

  if (dict->GetBoolean(config_keys::kLocalDiscoveryEnabled, &tmp_bool))
    set_local_discovery_enabled(tmp_bool);

  if (dict->GetBoolean(config_keys::kLocalPairingEnabled, &tmp_bool))
    set_local_pairing_enabled(tmp_bool);

  if (dict->GetString(config_keys::kRefreshToken, &tmp))
    set_refresh_token(tmp);

  if (dict->GetString(config_keys::kRobotAccount, &tmp))
    set_robot_account(tmp);

  if (dict->GetString(config_keys::kLastConfiguredSsid, &tmp))
    set_last_configured_ssid(tmp);

  if (dict->GetString(config_keys::kDeviceId, &tmp))
    set_device_id(tmp);
}

bool Config::Save() {
  if (!storage_)
    return false;
  base::DictionaryValue dict;
  dict.SetString(config_keys::kClientId, settings_.client_id);
  dict.SetString(config_keys::kClientSecret, settings_.client_secret);
  dict.SetString(config_keys::kApiKey, settings_.api_key);
  dict.SetString(config_keys::kOAuthURL, settings_.oauth_url);
  dict.SetString(config_keys::kServiceURL, settings_.service_url);
  dict.SetString(config_keys::kRefreshToken, settings_.refresh_token);
  dict.SetString(config_keys::kDeviceId, settings_.device_id);
  dict.SetString(config_keys::kRobotAccount, settings_.robot_account);
  dict.SetString(config_keys::kLastConfiguredSsid,
                 settings_.last_configured_ssid);
  dict.SetString(config_keys::kName, settings_.name);
  dict.SetString(config_keys::kDescription, settings_.description);
  dict.SetString(config_keys::kLocation, settings_.location);
  dict.SetString(config_keys::kLocalAnonymousAccessRole,
                 settings_.local_anonymous_access_role);
  dict.SetBoolean(config_keys::kLocalDiscoveryEnabled,
                  settings_.local_discovery_enabled);
  dict.SetBoolean(config_keys::kLocalPairingEnabled,
                  settings_.local_pairing_enabled);

  return storage_->Save(dict);
}

Config::Transaction::~Transaction() {
  Commit();
}

bool Config::Transaction::set_local_anonymous_access_role(
    const std::string& role) {
  if (!IsValidAccessRole(role)) {
    LOG(ERROR) << "Invalid role: " << role;
    return false;
  }
  settings_->local_anonymous_access_role = role;
  return true;
}

void Config::Transaction::Commit() {
  if (!config_)
    return;
  if (save_)
    config_->Save();
  for (const auto& cb : config_->on_changed_)
    cb.Run(*settings_);
  config_ = nullptr;
}

}  // namespace weave
