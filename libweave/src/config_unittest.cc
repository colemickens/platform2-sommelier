// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/config.h"

#include <set>

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <weave/mock_config_store.h>

#include "libweave/src/commands/unittest_utils.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace weave {

class ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*this, OnConfigChanged(_))
        .Times(1);  // Called from AddOnChangedCallback
    config_.reset(new Config{&config_store_});
    config_->AddOnChangedCallback(
        base::Bind(&ConfigTest::OnConfigChanged, base::Unretained(this)));
  }

  MOCK_METHOD1(OnConfigChanged, void(const Settings&));

  unittests::MockConfigStore config_store_;
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
  EXPECT_EQ("", config_->firmware_version());
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

TEST_F(ConfigTest, LoadState) {
  auto state = R"({
    "api_key": "state_api_key",
    "client_id": "state_client_id",
    "client_secret": "state_client_secret",
    "description": "state_description",
    "device_id": "state_device_id",
    "local_anonymous_access_role": "user",
    "local_discovery_enabled": false,
    "local_pairing_enabled": false,
    "location": "state_location",
    "name": "state_name",
    "oauth_url": "state_oauth_url",
    "refresh_token": "state_refresh_token",
    "robot_account": "state_robot_account",
    "last_configured_ssid": "state_last_configured_ssid",
    "service_url": "state_service_url"
  })";
  EXPECT_CALL(config_store_, LoadSettings()).WillOnce(Return(state));

  EXPECT_CALL(*this, OnConfigChanged(_)).Times(1);
  config_->Load();

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

  EXPECT_CALL(config_store_, SaveSettings(_))
      .WillOnce(Invoke([](const std::string& json) {
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
        EXPECT_JSON_EQ(expected, *unittests::CreateValue(json));
      }));
  EXPECT_CALL(config_store_, OnSettingsChanged(_)).Times(1);

  change.Commit();
}

}  // namespace weave
