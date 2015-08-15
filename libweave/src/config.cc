// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/config.h"

#include <set>

#include <base/bind.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <weave/enum_to_string.h>

#include "libweave/src/privet/privet_types.h"
#include "libweave/src/string_utils.h"

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
const char kRefreshToken[] = "refresh_token";
const char kDeviceId[] = "device_id";
const char kRobotAccount[] = "robot_account";
const char kLastConfiguredSsid[] = "last_configured_ssid";

}  // namespace config_keys

namespace {

bool IsValidAccessRole(const std::string& role) {
  privet::AuthScope scope;
  return StringToEnum(role, &scope);
}

Settings CreateDefaultSettings() {
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

}  // namespace

Config::Config(ConfigStore* config_store)
    : settings_{CreateDefaultSettings()}, config_store_{config_store} {
  if (config_store_) {
    AddOnChangedCallback(base::Bind(&ConfigStore::OnSettingsChanged,
                                    base::Unretained(config_store_)));
  }
}

void Config::AddOnChangedCallback(const OnChangedCallback& callback) {
  on_changed_.push_back(callback);
  // Force to read current state.
  callback.Run(settings_);
}

const Settings& Config::GetSettings() const {
  return settings_;
}

void Config::Load() {
  Transaction change{this};
  change.save_ = false;

  settings_ = CreateDefaultSettings();

  if (!config_store_)
    return;

  // Crash on any mistakes in defaults.
  CHECK(config_store_->LoadDefaults(&settings_));

  CHECK(!settings_.client_id.empty());
  CHECK(!settings_.client_secret.empty());
  CHECK(!settings_.api_key.empty());
  CHECK(!settings_.oauth_url.empty());
  CHECK(!settings_.service_url.empty());
  CHECK(!settings_.oem_name.empty());
  CHECK(!settings_.model_name.empty());
  CHECK(!settings_.model_id.empty());
  CHECK(!settings_.name.empty());
  CHECK(IsValidAccessRole(settings_.local_anonymous_access_role))
      << "Invalid role: " << settings_.local_anonymous_access_role;
  CHECK_EQ(
      settings_.embedded_code.empty(),
      std::find(settings_.pairing_modes.begin(), settings_.pairing_modes.end(),
                PairingType::kEmbeddedCode) == settings_.pairing_modes.end());

  change.LoadState();
}

void Config::Transaction::LoadState() {
  if (!config_->config_store_)
    return;
  std::string json_string = config_->config_store_->LoadSettings();
  if (json_string.empty())
    return;

  auto value = base::JSONReader::Read(json_string);
  const base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Failed to parse settings.";
    return;
  }

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

void Config::Save() {
  if (!config_store_)
    return;

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

  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);

  config_store_->SaveSettings(json_string);
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
