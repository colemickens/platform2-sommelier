// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

#include <map>
#include <set>
#include <utility>

#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <brillo/strings/string_utils.h>
#include <weave/enum_to_string.h>

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
const char kWifiAutoSetupEnabled[] = "wifi_auto_setup_enabled";
const char kEmbeddedCode[] = "embedded_code";
const char kPairingModes[] = "pairing_modes";

}  // namespace config_keys

BuffetConfig::BuffetConfig(const Options& options) : options_(options) {}

bool BuffetConfig::LoadDefaults(weave::Settings* settings) {
  // Keep this hardcoded default for sometime. This previously was set by
  // libweave. It should be set by overlay's buffet.conf.
  // Keys owners: avakulenko, gene, vitalybuka.
  settings->client_id =
      "338428340000-vkb4p6h40c7kja1k3l70kke8t615cjit.apps.googleusercontent."
      "com";
  settings->client_secret = "LS_iPYo_WIOE0m2VnLdduhnx";
  settings->api_key = "AIzaSyACK3oZtmIylUKXiTMqkZqfuRiCgQmQSAQ";

  settings->name = "Developer device";
  settings->oem_name = "Chromium";
  settings->model_name = "Brillo";
  settings->model_id = "AAAAA";

  if (!base::PathExists(options_.defaults))
    return true;  // Nothing to load.

  brillo::KeyValueStore store;
  if (!store.Load(options_.defaults))
    return false;
  bool result = LoadDefaults(store, settings);
  settings->disable_security = options_.disable_security;
  settings->test_privet_ssid = options_.test_privet_ssid;

  if (!options_.client_id.empty())
    settings->client_id = options_.client_id;
  if (!options_.client_secret.empty())
    settings->client_secret = options_.client_secret;
  if (!options_.api_key.empty())
    settings->api_key = options_.api_key;
  if (!options_.oauth_url.empty())
    settings->oauth_url = options_.oauth_url;
  if (!options_.service_url.empty())
    settings->service_url = options_.service_url;

  return result;
}

bool BuffetConfig::LoadDefaults(const brillo::KeyValueStore& store,
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
  brillo::KeyValueStore lsb_release_store;
  if (lsb_release_store.Load(lsb_release_path) &&
      lsb_release_store.GetString("CHROMEOS_RELEASE_VERSION",
                                  &settings->firmware_version)) {
  } else {
    LOG(ERROR) << "Failed to get CHROMEOS_RELEASE_VERSION from "
               << lsb_release_path.value();
  }

  store.GetBoolean(config_keys::kWifiAutoSetupEnabled,
                   &settings->wifi_auto_setup_enabled);
  store.GetString(config_keys::kEmbeddedCode, &settings->embedded_code);

  std::string modes_str;
  if (store.GetString(config_keys::kPairingModes, &modes_str)) {
    std::set<weave::PairingType> pairing_modes;
    for (const std::string& mode :
         brillo::string_utils::Split(modes_str, ",", true, true)) {
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

  std::string role_str;
  if (store.GetString(config_keys::kLocalAnonymousAccessRole, &role_str)) {
    if (!StringToEnum(role_str, &settings->local_anonymous_access_role))
      return false;
  }
  store.GetBoolean(config_keys::kLocalDiscoveryEnabled,
                   &settings->local_discovery_enabled);
  store.GetBoolean(config_keys::kLocalPairingEnabled,
                   &settings->local_pairing_enabled);
  return true;
}

std::string BuffetConfig::LoadSettings() {
  std::string json_string;
  base::ReadFileToString(options_.settings, &json_string);
  return json_string;
}

void BuffetConfig::SaveSettings(const std::string& settings) {
  base::ImportantFileWriter::WriteFileAtomically(options_.settings, settings);
}

}  // namespace buffet
