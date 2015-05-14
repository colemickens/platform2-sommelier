// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include "buffet/storage_impls.h"
#include "buffet/storage_interface.h"

namespace {

// TODO(vitalybuka): Remove this when deviceKind is gone from server.
std::string GetDeviceKind(const std::string& manifest_id) {
  CHECK_EQ(5u, manifest_id.size());
  std::string kind = manifest_id.substr(0, 2);
  if (kind == "AC")
    return "accessPoint";
  if (kind == "AK")
    return "aggregator";
  if (kind == "AM")
    return "camera";
  if (kind == "AB")
    return "developmentBoard";
  if (kind == "AE")
    return "printer";
  if (kind == "AF")
    return "scanner";
  if (kind == "AD")
    return "speaker";
  if (kind == "AL")
    return "storage";
  if (kind == "AJ")
    return "toy";
  if (kind == "AA")
    return "vendor";
  if (kind == "AN")
    return "video";
  LOG(FATAL) << "Invalid model id: " << manifest_id;
  return std::string();
}

bool IsValidAccessRole(const std::string& role) {
  return role == "none" || role == "viewer" || role == "user";
}

}  // namespace

namespace buffet {

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
const char kRefreshToken[] = "refresh_token";
const char kDeviceId[] = "device_id";
const char kRobotAccount[] = "robot_account";

}  // namespace config_keys

BuffetConfig::BuffetConfig(std::unique_ptr<StorageInterface> storage)
    : storage_{std::move(storage)} {
}

BuffetConfig::BuffetConfig(const base::FilePath& state_path)
    : BuffetConfig{
          std::unique_ptr<StorageInterface>{new FileStorage{state_path}}} {
}

void BuffetConfig::Load(const base::FilePath& config_path) {
  chromeos::KeyValueStore store;
  if (store.Load(config_path)) {
    Load(store);
  }
}

void BuffetConfig::Load(const chromeos::KeyValueStore& store) {
  Transaction change{this};
  change.save_ = false;

  store.GetString(config_keys::kClientId, &client_id_);
  CHECK(!client_id_.empty());

  store.GetString(config_keys::kClientSecret, &client_secret_);
  CHECK(!client_secret_.empty());

  store.GetString(config_keys::kApiKey, &api_key_);
  CHECK(!api_key_.empty());

  store.GetString(config_keys::kOAuthURL, &oauth_url_);
  CHECK(!oauth_url_.empty());

  store.GetString(config_keys::kServiceURL, &service_url_);
  CHECK(!service_url_.empty());

  store.GetString(config_keys::kOemName, &oem_name_);
  CHECK(!oem_name_.empty());

  store.GetString(config_keys::kModelName, &model_name_);
  CHECK(!model_name_.empty());

  store.GetString(config_keys::kModelId, &model_id_);
  device_kind_ = GetDeviceKind(model_id_);

  std::string polling_period_str;
  if (store.GetString(config_keys::kPollingPeriodMs, &polling_period_str))
    CHECK(base::StringToUint64(polling_period_str, &polling_period_ms_));

  store.GetString(config_keys::kName, &name_);
  CHECK(!name_.empty());

  store.GetString(config_keys::kDescription, &description_);
  store.GetString(config_keys::kLocation, &location_);

  store.GetString(config_keys::kLocalAnonymousAccessRole,
                  &local_anonymous_access_role_);
  CHECK(IsValidAccessRole(local_anonymous_access_role_))
      << "Invalid role: " << local_anonymous_access_role_;

  store.GetBoolean(config_keys::kLocalDiscoveryEnabled,
                   &local_discovery_enabled_);
  store.GetBoolean(config_keys::kLocalPairingEnabled, &local_pairing_enabled_);

  change.LoadState();
}

void BuffetConfig::Transaction::LoadState() {
  if (!config_->storage_)
    return;
  auto value = config_->storage_->Load();
  const base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict))
    return;

  std::string name;
  if (dict->GetString(config_keys::kName, &name))
    set_name(name);

  std::string description;
  if (dict->GetString(config_keys::kDescription, &description))
    set_description(description);

  std::string location;
  if (dict->GetString(config_keys::kLocation, &location))
    set_location(location);

  std::string access_role;
  if (dict->GetString(config_keys::kLocalAnonymousAccessRole, &access_role))
    set_local_anonymous_access_role(access_role);

  bool discovery_enabled{false};
  if (dict->GetBoolean(config_keys::kLocalDiscoveryEnabled, &discovery_enabled))
    set_local_discovery_enabled(discovery_enabled);

  bool pairing_enabled{false};
  if (dict->GetBoolean(config_keys::kLocalPairingEnabled, &pairing_enabled))
    set_local_pairing_enabled(pairing_enabled);

  std::string token;
  if (dict->GetString(config_keys::kRefreshToken, &token))
    set_refresh_token(token);

  std::string account;
  if (dict->GetString(config_keys::kRobotAccount, &account))
    set_robot_account(account);

  std::string device_id;
  if (dict->GetString(config_keys::kDeviceId, &device_id))
    set_device_id(device_id);
}

bool BuffetConfig::Save() {
  if (!storage_)
    return false;
  base::DictionaryValue dict;
  dict.SetString(config_keys::kRefreshToken, refresh_token_);
  dict.SetString(config_keys::kDeviceId, device_id_);
  dict.SetString(config_keys::kRobotAccount, robot_account_);
  dict.SetString(config_keys::kName, name_);
  dict.SetString(config_keys::kDescription, description_);
  dict.SetString(config_keys::kLocation, location_);
  dict.SetString(config_keys::kLocalAnonymousAccessRole,
                 local_anonymous_access_role_);
  dict.SetBoolean(config_keys::kLocalDiscoveryEnabled,
                  local_discovery_enabled_);
  dict.SetBoolean(config_keys::kLocalPairingEnabled, local_pairing_enabled_);

  return storage_->Save(dict);
}

BuffetConfig::Transaction::~Transaction() {
  Commit();
}

bool BuffetConfig::Transaction::set_name(const std::string& name) {
  if (name.empty()) {
    LOG(ERROR) << "Invalid name: " << name;
    return false;
  }
  config_->name_ = name;
  return true;
}

bool BuffetConfig::Transaction::set_local_anonymous_access_role(
    const std::string& role) {
  if (!IsValidAccessRole(role)) {
    LOG(ERROR) << "Invalid role: " << role;
    return false;
  }
  config_->local_anonymous_access_role_ = role;
  return true;
}

void BuffetConfig::Transaction::Commit() {
  if (!config_)
    return;
  if (save_)
    config_->Save();
  for (const auto& cb : config_->on_changed_)
    cb.Run(*config_);
  config_ = nullptr;
}

}  // namespace buffet
