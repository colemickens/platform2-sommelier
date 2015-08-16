// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/config.h"

#include <set>

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "libweave/src/commands/unittest_utils.h"
#include "libweave/src/storage_impls.h"

using testing::_;

namespace weave {

class ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*this, OnConfigChanged(_))
        .Times(1);  // Called from AddOnChangedCallback

    std::unique_ptr<StorageInterface> storage{new MemStorage};
    storage_ = storage.get();
    config_.reset(new Config{std::move(storage)});

    config_->AddOnChangedCallback(
        base::Bind(&ConfigTest::OnConfigChanged, base::Unretained(this)));
  }

  MOCK_METHOD1(OnConfigChanged, void(const Settings&));

  StorageInterface* storage_{nullptr};
  std::unique_ptr<Config> config_;
  const Config default_{nullptr};
};

TEST_F(ConfigTest, NoStorage) {
  Config config{nullptr};
  Config::Transaction change{&config};
  change.Commit();
}

TEST_F(ConfigTest, Defaults) {
  EXPECT_EQ("58855907228.apps.googleusercontent.com", config_->client_id());
  EXPECT_EQ("eHSAREAHrIqPsHBxCE9zPPBi", config_->client_secret());
  EXPECT_EQ("AIzaSyDSq46gG-AxUnC3zoqD9COIPrjolFsMfMA", config_->api_key());
  EXPECT_EQ("https://accounts.google.com/o/oauth2/", config_->oauth_url());
  EXPECT_EQ("https://www.googleapis.com/clouddevices/v1/",
            config_->service_url());
  EXPECT_EQ("Chromium", config_->oem_name());
  EXPECT_EQ("Brillo", config_->model_name());
  EXPECT_EQ("AAAAA", config_->model_id());
  EXPECT_EQ(base::TimeDelta::FromSeconds(7), config_->polling_period());
  EXPECT_EQ(base::TimeDelta::FromMinutes(30), config_->backup_polling_period());
  EXPECT_TRUE(config_->wifi_auto_setup_enabled());
  EXPECT_FALSE(config_->ble_setup_enabled());
  EXPECT_EQ(std::set<PairingType>{PairingType::kPinCode},
            config_->pairing_modes());
  EXPECT_EQ("", config_->embedded_code());
  EXPECT_EQ("Developer device", config_->name());
  EXPECT_EQ("", config_->description());
  EXPECT_EQ("", config_->location());
  EXPECT_EQ("viewer", config_->local_anonymous_access_role());
  EXPECT_TRUE(config_->local_pairing_enabled());
  EXPECT_TRUE(config_->local_discovery_enabled());
  EXPECT_EQ("", config_->device_id());
  EXPECT_EQ("", config_->refresh_token());
  EXPECT_EQ("", config_->robot_account());
  EXPECT_EQ("", config_->last_configured_ssid());
}

TEST_F(ConfigTest, LoadConfig) {
  chromeos::KeyValueStore config_store;
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
  config_store.SetString("pairing_modes",
                         "pinCode,embeddedCode,ultrasound32,audible32");
  config_store.SetString("embedded_code", "1234");
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

  EXPECT_CALL(*this, OnConfigChanged(_)).Times(1);
  config_->Load(config_store);

  EXPECT_EQ("conf_client_id", config_->client_id());
  EXPECT_EQ("conf_client_secret", config_->client_secret());
  EXPECT_EQ("conf_api_key", config_->api_key());
  EXPECT_EQ("conf_oauth_url", config_->oauth_url());
  EXPECT_EQ("conf_service_url", config_->service_url());
  EXPECT_EQ("conf_oem_name", config_->oem_name());
  EXPECT_EQ("conf_model_name", config_->model_name());
  EXPECT_EQ("ABCDE", config_->model_id());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(12345),
            config_->polling_period());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(6589),
            config_->backup_polling_period());
  EXPECT_FALSE(config_->wifi_auto_setup_enabled());
  EXPECT_TRUE(config_->ble_setup_enabled());
  std::set<PairingType> pairing_types{
      PairingType::kPinCode, PairingType::kEmbeddedCode,
      PairingType::kUltrasound32, PairingType::kAudible32};
  EXPECT_EQ(pairing_types, config_->pairing_modes());
  EXPECT_EQ("1234", config_->embedded_code());
  EXPECT_EQ("conf_name", config_->name());
  EXPECT_EQ("conf_description", config_->description());
  EXPECT_EQ("conf_location", config_->location());
  EXPECT_EQ("user", config_->local_anonymous_access_role());
  EXPECT_FALSE(config_->local_pairing_enabled());
  EXPECT_FALSE(config_->local_discovery_enabled());

  // Not from config.
  EXPECT_EQ(default_.device_id(), config_->device_id());
  EXPECT_EQ(default_.refresh_token(), config_->refresh_token());
  EXPECT_EQ(default_.robot_account(), config_->robot_account());
  EXPECT_EQ(default_.last_configured_ssid(), config_->last_configured_ssid());

  // Nothing should be saved yet.
  EXPECT_JSON_EQ("{}", *storage_->Load());

  Config::Transaction change{config_.get()};
  EXPECT_CALL(*this, OnConfigChanged(_)).Times(1);
  change.Commit();

  auto expected = R"({
    'api_key': 'conf_api_key',
    'client_id': 'conf_client_id',
    'client_secret': 'conf_client_secret',
    'description': 'conf_description',
    'device_id': '',
    'local_anonymous_access_role': 'user',
    'local_discovery_enabled': false,
    'local_pairing_enabled': false,
    'location': 'conf_location',
    'name': 'conf_name',
    'oauth_url': 'conf_oauth_url',
    'refresh_token': '',
    'robot_account': '',
    'last_configured_ssid': '',
    'service_url': 'conf_service_url'
  })";
  EXPECT_JSON_EQ(expected, *storage_->Load());
}

TEST_F(ConfigTest, LoadState) {
  auto state = R"({
    'api_key': 'state_api_key',
    'client_id': 'state_client_id',
    'client_secret': 'state_client_secret',
    'description': 'state_description',
    'device_id': 'state_device_id',
    'local_anonymous_access_role': 'user',
    'local_discovery_enabled': false,
    'local_pairing_enabled': false,
    'location': 'state_location',
    'name': 'state_name',
    'oauth_url': 'state_oauth_url',
    'refresh_token': 'state_refresh_token',
    'robot_account': 'state_robot_account',
    'last_configured_ssid': 'state_last_configured_ssid',
    'service_url': 'state_service_url'
  })";
  storage_->Save(*unittests::CreateDictionaryValue(state));

  chromeos::KeyValueStore config_store;
  EXPECT_CALL(*this, OnConfigChanged(_)).Times(1);
  config_->Load(config_store);

  // Clear storage.
  storage_->Save(base::DictionaryValue());

  EXPECT_EQ("state_client_id", config_->client_id());
  EXPECT_EQ("state_client_secret", config_->client_secret());
  EXPECT_EQ("state_api_key", config_->api_key());
  EXPECT_EQ("state_oauth_url", config_->oauth_url());
  EXPECT_EQ("state_service_url", config_->service_url());
  EXPECT_EQ(default_.oem_name(), config_->oem_name());
  EXPECT_EQ(default_.model_name(), config_->model_name());
  EXPECT_EQ(default_.model_id(), config_->model_id());
  EXPECT_EQ(default_.polling_period(), config_->polling_period());
  EXPECT_EQ(default_.backup_polling_period(), config_->backup_polling_period());
  EXPECT_EQ(default_.wifi_auto_setup_enabled(),
            config_->wifi_auto_setup_enabled());
  EXPECT_EQ(default_.ble_setup_enabled(), config_->ble_setup_enabled());
  EXPECT_EQ(default_.pairing_modes(), config_->pairing_modes());
  EXPECT_EQ(default_.embedded_code(), config_->embedded_code());
  EXPECT_EQ("state_name", config_->name());
  EXPECT_EQ("state_description", config_->description());
  EXPECT_EQ("state_location", config_->location());
  EXPECT_EQ("user", config_->local_anonymous_access_role());
  EXPECT_FALSE(config_->local_pairing_enabled());
  EXPECT_FALSE(config_->local_discovery_enabled());
  EXPECT_EQ("state_device_id", config_->device_id());
  EXPECT_EQ("state_refresh_token", config_->refresh_token());
  EXPECT_EQ("state_robot_account", config_->robot_account());
  EXPECT_EQ("state_last_configured_ssid", config_->last_configured_ssid());

  // Nothing should be saved yet.
  EXPECT_JSON_EQ("{}", *storage_->Load());

  Config::Transaction change{config_.get()};
  EXPECT_CALL(*this, OnConfigChanged(_)).Times(1);
  change.Commit();

  EXPECT_JSON_EQ(state, *storage_->Load());
}

TEST_F(ConfigTest, Setters) {
  Config::Transaction change{config_.get()};

  change.set_client_id("set_client_id");
  EXPECT_EQ("set_client_id", config_->client_id());

  change.set_client_secret("set_client_secret");
  EXPECT_EQ("set_client_secret", config_->client_secret());

  change.set_api_key("set_api_key");
  EXPECT_EQ("set_api_key", config_->api_key());

  change.set_oauth_url("set_oauth_url");
  EXPECT_EQ("set_oauth_url", config_->oauth_url());

  change.set_service_url("set_service_url");
  EXPECT_EQ("set_service_url", config_->service_url());

  change.set_name("set_name");
  EXPECT_EQ("set_name", config_->name());

  change.set_description("set_description");
  EXPECT_EQ("set_description", config_->description());

  change.set_location("set_location");
  EXPECT_EQ("set_location", config_->location());

  change.set_local_anonymous_access_role("viewer");
  EXPECT_EQ("viewer", config_->local_anonymous_access_role());

  change.set_local_anonymous_access_role("none");
  EXPECT_EQ("none", config_->local_anonymous_access_role());

  change.set_local_anonymous_access_role("user");
  EXPECT_EQ("user", config_->local_anonymous_access_role());

  change.set_local_discovery_enabled(false);
  EXPECT_FALSE(config_->local_discovery_enabled());

  change.set_local_pairing_enabled(false);
  EXPECT_FALSE(config_->local_pairing_enabled());

  change.set_local_discovery_enabled(true);
  EXPECT_TRUE(config_->local_discovery_enabled());

  change.set_local_pairing_enabled(true);
  EXPECT_TRUE(config_->local_pairing_enabled());

  change.set_device_id("set_id");
  EXPECT_EQ("set_id", config_->device_id());

  change.set_refresh_token("set_token");
  EXPECT_EQ("set_token", config_->refresh_token());

  change.set_robot_account("set_account");
  EXPECT_EQ("set_account", config_->robot_account());

  change.set_last_configured_ssid("set_last_configured_ssid");
  EXPECT_EQ("set_last_configured_ssid", config_->last_configured_ssid());

  EXPECT_CALL(*this, OnConfigChanged(_)).Times(1);
  change.Commit();

  auto expected = R"({
    'api_key': 'set_api_key',
    'client_id': 'set_client_id',
    'client_secret': 'set_client_secret',
    'description': 'set_description',
    'device_id': 'set_id',
    'local_anonymous_access_role': 'user',
    'local_discovery_enabled': true,
    'local_pairing_enabled': true,
    'location': 'set_location',
    'name': 'set_name',
    'oauth_url': 'set_oauth_url',
    'refresh_token': 'set_token',
    'robot_account': 'set_account',
    'last_configured_ssid': 'set_last_configured_ssid',
    'service_url': 'set_service_url'
  })";
  EXPECT_JSON_EQ(expected, *storage_->Load());
}
}  // namespace weave
