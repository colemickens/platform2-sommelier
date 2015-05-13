// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

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
const char kAnonymousAccessRole[] = "anonymous_access_role";
const char kOemName[] = "oem_name";
const char kModelName[] = "model_name";
const char kModelId[] = "model_id";
const char kPollingPeriodMs[] = "polling_period_ms";

}  // namespace config_keys

void BuffetConfig::Load(const base::FilePath& config_path) {
  chromeos::KeyValueStore store;
  if (store.Load(config_path)) {
    Load(store);
  }
}

void BuffetConfig::Load(const chromeos::KeyValueStore& store) {
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

  store.GetString(config_keys::kAnonymousAccessRole, &anonymous_access_role_);
  CHECK(IsValidAccessRole(anonymous_access_role_))
      << "Invalid role: " << anonymous_access_role_;
}

void BuffetConfig::set_name(const std::string& name) {
  CHECK(!name.empty());
  name_ = name;
}

void BuffetConfig::set_anonymous_access_role(const std::string& role) {
  if (IsValidAccessRole(role)) {
    anonymous_access_role_ = role;
  } else {
    LOG(ERROR) << "Invalid role: " << role;
  }
}

}  // namespace buffet
