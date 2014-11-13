// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/config.h"

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace apmanager {

namespace {

const char kServicePath[] = "/manager/services/0";
const char kSsid[] = "TestSsid";
const char kInterface[] = "uap0";
const char kPassphrase[] = "Passphrase";
const uint16_t k24GHzChannel = 6;
const uint16_t k5GHzChannel = 36;

const char kExpected80211gConfigContent[] = "ssid=TestSsid\n"
                                            "channel=6\n"
                                            "hw_mode=g\n"
                                            "interface=uap0\n"
                                            "driver=nl80211\n"
                                            "fragm_threshold=2346\n"
                                            "rts_threshold=2347\n";

const char kExpected80211n5GHzConfigContent[] = "ssid=TestSsid\n"
                                                "channel=36\n"
                                                "ieee80211n=1\n"
                                                "hw_mode=a\n"
                                                "interface=uap0\n"
                                                "driver=nl80211\n"
                                                "fragm_threshold=2346\n"
                                                "rts_threshold=2347\n";

const char kExpected80211n24GHzConfigContent[] = "ssid=TestSsid\n"
                                                 "channel=6\n"
                                                 "ieee80211n=1\n"
                                                 "hw_mode=g\n"
                                                 "interface=uap0\n"
                                                 "driver=nl80211\n"
                                                 "fragm_threshold=2346\n"
                                                 "rts_threshold=2347\n";

const char kExpectedRsnConfigContent[] = "ssid=TestSsid\n"
                                         "channel=6\n"
                                         "hw_mode=g\n"
                                         "interface=uap0\n"
                                         "wpa=2\n"
                                         "rsn_pairwise=CCMP\n"
                                         "wpa_key_mgmt=WPA-PSK\n"
                                         "wpa_passphrase=Passphrase\n"
                                         "driver=nl80211\n"
                                         "fragm_threshold=2346\n"
                                         "rts_threshold=2347\n";

}  // namespace

class ConfigTest : public testing::Test {
 public:
  ConfigTest() : config_(kServicePath) {}

 protected:
  Config config_;
};

MATCHER_P(IsConfigErrorStartingWith, message, "") {
  return arg != nullptr &&
      arg->GetDomain() == chromeos::errors::dbus::kDomain &&
      arg->GetCode() == kConfigError &&
      StartsWithASCII(arg->GetMessage(), message, false);
}

TEST_F(ConfigTest, NoSsid) {
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);

  std::string config_content;
  chromeos::ErrorPtr error;
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error, IsConfigErrorStartingWith("SSID not specified"));
}

TEST_F(ConfigTest, 80211gConfig) {
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);

  std::string config_content;
  chromeos::ErrorPtr error;
  EXPECT_TRUE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_NE(std::string::npos, config_content.find(
                                   kExpected80211gConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211gConfigContent << "..within content...\n"
      << config_content;
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(ConfigTest, 80211nConfig) {
  config_.SetSsid(kSsid);
  config_.SetHwMode(kHwMode80211n);
  config_.SetInterfaceName(kInterface);

  // 5GHz channel.
  config_.SetChannel(k5GHzChannel);
  std::string ghz5_config_content;
  chromeos::ErrorPtr error;
  EXPECT_TRUE(config_.GenerateConfigFile(&error, &ghz5_config_content));
  EXPECT_NE(std::string::npos, ghz5_config_content.find(
                                   kExpected80211n5GHzConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211n5GHzConfigContent << "..within content...\n"
      << ghz5_config_content;
  EXPECT_EQ(nullptr, error.get());

  // 2.4GHz channel.
  config_.SetChannel(k24GHzChannel);
  std::string ghz24_config_content;
  chromeos::ErrorPtr error1;
  EXPECT_TRUE(config_.GenerateConfigFile(&error1, &ghz24_config_content));
  EXPECT_NE(std::string::npos, ghz24_config_content.find(
                                   kExpected80211n24GHzConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211n24GHzConfigContent << "..within content...\n"
      << ghz24_config_content;
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(ConfigTest, RsnConfig) {
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);
  config_.SetSecurityMode(kSecurityModeRSN);

  // Failed due to no passphrase specified.
  std::string config_content;
  chromeos::ErrorPtr error;
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error, IsConfigErrorStartingWith(
      base::StringPrintf("Passphrase not set for security mode: %s",
                         kSecurityModeRSN)));

  chromeos::ErrorPtr error1;
  config_.SetPassphrase(kPassphrase);
  EXPECT_TRUE(config_.GenerateConfigFile(&error1, &config_content));
  EXPECT_NE(std::string::npos, config_content.find(
                                   kExpectedRsnConfigContent))
      << "Expected to find the following config...\n"
      << kExpectedRsnConfigContent << "..within content...\n"
      << config_content;
  EXPECT_EQ(nullptr, error1.get());
}

}  // namespace apmanager
