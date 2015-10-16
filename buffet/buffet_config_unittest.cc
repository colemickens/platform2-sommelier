// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/buffet_config.h"

#include <set>

#include <base/bind.h>
#include <gtest/gtest.h>

namespace buffet {

TEST(BuffetConfigTest, LoadConfig) {
  brillo::KeyValueStore config_store;
  config_store.SetString("client_id", "conf_client_id");
  config_store.SetString("client_secret", "conf_client_secret");
  config_store.SetString("api_key", "conf_api_key");
  config_store.SetString("oauth_url", "conf_oauth_url");
  config_store.SetString("service_url", "conf_service_url");
  config_store.SetString("oem_name", "conf_oem_name");
  config_store.SetString("model_name", "conf_model_name");
  config_store.SetString("model_id", "ABCDE");
  config_store.SetString("polling_period_ms", "12345");
  config_store.SetString("backup_polling_period_ms", "6589");
  config_store.SetBoolean("wifi_auto_setup_enabled", false);
  config_store.SetBoolean("ble_setup_enabled", true);
  config_store.SetString("pairing_modes", "pinCode,embeddedCode");
  config_store.SetString("embedded_code", "567");
  config_store.SetString("name", "conf_name");
  config_store.SetString("description", "conf_description");
  config_store.SetString("location", "conf_location");
  config_store.SetString("local_anonymous_access_role", "user");
  config_store.SetBoolean("local_pairing_enabled", false);
  config_store.SetBoolean("local_discovery_enabled", false);

  // Following will be ignored.
  config_store.SetString("device_kind", "conf_device_kind");
  config_store.SetString("device_id", "conf_device_id");
  config_store.SetString("refresh_token", "conf_refresh_token");
  config_store.SetString("robot_account", "conf_robot_account");
  config_store.SetString("last_configured_ssid", "conf_last_configured_ssid");

  weave::Settings settings;
  BuffetConfig config{{}};
  EXPECT_TRUE(config.LoadDefaults(config_store, &settings));

  EXPECT_EQ("conf_client_id", settings.client_id);
  EXPECT_EQ("conf_client_secret", settings.client_secret);
  EXPECT_EQ("conf_api_key", settings.api_key);
  EXPECT_EQ("conf_oauth_url", settings.oauth_url);
  EXPECT_EQ("conf_service_url", settings.service_url);
  EXPECT_EQ("conf_oem_name", settings.oem_name);
  EXPECT_EQ("conf_model_name", settings.model_name);
  EXPECT_EQ("ABCDE", settings.model_id);
  EXPECT_FALSE(settings.wifi_auto_setup_enabled);
  std::set<weave::PairingType> pairing_types{weave::PairingType::kPinCode,
                                             weave::PairingType::kEmbeddedCode};
  EXPECT_EQ(pairing_types, settings.pairing_modes);
  EXPECT_EQ("567", settings.embedded_code);
  EXPECT_EQ("conf_name", settings.name);
  EXPECT_EQ("conf_description", settings.description);
  EXPECT_EQ("conf_location", settings.location);
  EXPECT_EQ(weave::AuthScope::kUser, settings.local_anonymous_access_role);
  EXPECT_FALSE(settings.local_pairing_enabled);
  EXPECT_FALSE(settings.local_discovery_enabled);
}

}  // namespace buffet
