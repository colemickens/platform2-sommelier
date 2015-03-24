// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

namespace buffet {
namespace config_keys {

const char kClientId[]             = "client_id";
const char kClientSecret[]         = "client_secret";
const char kApiKey[]               = "api_key";
const char kOAuthURL[]             = "oauth_url";
const char kServiceURL[]           = "service_url";
const char kDeviceKind[]           = "device_kind";
const char kName[]                 = "name";
const char kDefaultDisplayName[]   = "default_display_name";
const char kDefaultDescription[]   = "default_description";
const char kDefaultLocation[]      = "default_location";
const char kModelId[]              = "model_id";

}  // namespace config_keys

void BuffetConfig::Load(const base::FilePath& config_path) {
  chromeos::KeyValueStore store;
  if (store.Load(config_path)) {
    Load(store);
  }
}

void BuffetConfig::Load(const chromeos::KeyValueStore& store) {
  store.GetString(config_keys::kClientId, &client_id_);
  store.GetString(config_keys::kClientSecret, &client_secret_);
  store.GetString(config_keys::kApiKey, &api_key_);
  store.GetString(config_keys::kOAuthURL, &oauth_url_);
  store.GetString(config_keys::kServiceURL, &service_url_);
  store.GetString(config_keys::kDeviceKind, &device_kind_);
  store.GetString(config_keys::kName, &name_);
  store.GetString(config_keys::kDefaultDisplayName, &default_display_name_);
  store.GetString(config_keys::kDefaultDescription, &default_description_);
  store.GetString(config_keys::kDefaultLocation, &default_location_);
  store.GetString(config_keys::kModelId, &model_id_);
}

}  // namespace buffet
