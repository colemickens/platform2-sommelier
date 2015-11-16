// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/config.h"

#include <string>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#if !defined(__ANDROID__)
#include <chromeos/dbus/service_constants.h>
#else
#include "dbus/apmanager/dbus-constants.h"
#endif

#include "apmanager/fake_device_adaptor.h"
#include "apmanager/mock_control.h"
#include "apmanager/mock_device.h"
#include "apmanager/mock_manager.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::SetArgumentPointee;
namespace apmanager {

namespace {

const char kServicePath[] = "/manager/services/0";
const char kSsid[] = "TestSsid";
const char kInterface[] = "uap0";
const char kBridgeInterface[] = "br0";
const char kControlInterfacePath[] = "/var/run/apmanager/hostapd/ctrl_iface";
const char kPassphrase[] = "Passphrase";
const char k24GHzHTCapab[] = "[LDPC SMPS-STATIC GF SHORT-GI-20]";
const char k5GHzHTCapab[] =
    "[LDPC HT40+ SMPS-STATIC GF SHORT-GI-20 SHORT-GI-40]";

const uint16_t k24GHzChannel = 6;
const uint16_t k5GHzChannel = 36;

const char kExpected80211gConfigContent[] = "ssid=TestSsid\n"
                                            "channel=6\n"
                                            "interface=uap0\n"
                                            "hw_mode=g\n"
                                            "driver=nl80211\n"
                                            "fragm_threshold=2346\n"
                                            "rts_threshold=2347\n";

const char kExpected80211gBridgeConfigContent[] = "ssid=TestSsid\n"
                                                  "bridge=br0\n"
                                                  "channel=6\n"
                                                  "interface=uap0\n"
                                                  "hw_mode=g\n"
                                                  "driver=nl80211\n"
                                                  "fragm_threshold=2346\n"
                                                  "rts_threshold=2347\n";

const char kExpected80211gCtrlIfaceConfigContent[] =
    "ssid=TestSsid\n"
    "channel=6\n"
    "interface=uap0\n"
    "hw_mode=g\n"
    "ctrl_interface=/var/run/apmanager/hostapd/ctrl_iface\n"
#if !defined(__ANDROID__)
    "ctrl_interface_group=apmanager\n"
#else
    "ctrl_interface_group=system\n"
#endif  // __ANDROID__
    "driver=nl80211\n"
    "fragm_threshold=2346\n"
    "rts_threshold=2347\n";

const char kExpected80211n5GHzConfigContent[] =
    "ssid=TestSsid\n"
    "channel=36\n"
    "interface=uap0\n"
    "ieee80211n=1\n"
    "ht_capab=[LDPC HT40+ SMPS-STATIC GF SHORT-GI-20 SHORT-GI-40]\n"
    "hw_mode=a\n"
    "driver=nl80211\n"
    "fragm_threshold=2346\n"
    "rts_threshold=2347\n";

const char kExpected80211n24GHzConfigContent[] =
    "ssid=TestSsid\n"
    "channel=6\n"
    "interface=uap0\n"
    "ieee80211n=1\n"
    "ht_capab=[LDPC SMPS-STATIC GF SHORT-GI-20]\n"
    "hw_mode=g\n"
    "driver=nl80211\n"
    "fragm_threshold=2346\n"
    "rts_threshold=2347\n";

const char kExpectedRsnConfigContent[] = "ssid=TestSsid\n"
                                         "channel=6\n"
                                         "interface=uap0\n"
                                         "hw_mode=g\n"
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
  ConfigTest()
      : manager_(&control_interface_),
        config_(&manager_, kServicePath) {
    ON_CALL(control_interface_, CreateDeviceAdaptorRaw())
        .WillByDefault(ReturnNew<FakeDeviceAdaptor>());
  }

  void SetupDevice(const std::string& interface) {
    // Setup mock device.
    device_ = new MockDevice(&manager_);
    device_->SetPreferredApInterface(interface);
    EXPECT_CALL(manager_, GetDeviceFromInterfaceName(interface))
        .WillRepeatedly(Return(device_));
  }

 protected:
  MockControl control_interface_;
  MockManager manager_;
  scoped_refptr<MockDevice> device_;
  Config config_;
};

MATCHER_P(IsConfigErrorStartingWith, message, "") {
  return arg != nullptr &&
         arg->GetDomain() == brillo::errors::dbus::kDomain &&
         arg->GetCode() == kConfigError &&
         base::StartsWithASCII(arg->GetMessage(), message, false);
}

TEST_F(ConfigTest, GetFrequencyFromChannel) {
  uint32_t frequency;
  // Invalid channel.
  EXPECT_FALSE(Config::GetFrequencyFromChannel(0, &frequency));
  EXPECT_FALSE(Config::GetFrequencyFromChannel(166, &frequency));
  EXPECT_FALSE(Config::GetFrequencyFromChannel(14, &frequency));
  EXPECT_FALSE(Config::GetFrequencyFromChannel(33, &frequency));

  // Valid channel.
  const uint32_t kChannel1Frequency = 2412;
  const uint32_t kChannel13Frequency = 2472;
  const uint32_t kChannel34Frequency = 5170;
  const uint32_t kChannel165Frequency = 5825;
  EXPECT_TRUE(Config::GetFrequencyFromChannel(1, &frequency));
  EXPECT_EQ(kChannel1Frequency, frequency);
  EXPECT_TRUE(Config::GetFrequencyFromChannel(13, &frequency));
  EXPECT_EQ(kChannel13Frequency, frequency);
  EXPECT_TRUE(Config::GetFrequencyFromChannel(34, &frequency));
  EXPECT_EQ(kChannel34Frequency, frequency);
  EXPECT_TRUE(Config::GetFrequencyFromChannel(165, &frequency));
  EXPECT_EQ(kChannel165Frequency, frequency);
}

TEST_F(ConfigTest, ValidateSsid) {
  brillo::ErrorPtr error;
  // SSID must contain between 1 and 32 characters.
  EXPECT_TRUE(config_.ValidateSsid(&error, "s"));
  EXPECT_TRUE(config_.ValidateSsid(&error, std::string(32, 'c')));
  EXPECT_FALSE(config_.ValidateSsid(&error, ""));
  EXPECT_FALSE(config_.ValidateSsid(&error, std::string(33, 'c')));
}

TEST_F(ConfigTest, ValidateSecurityMode) {
  brillo::ErrorPtr error;
  EXPECT_TRUE(config_.ValidateSecurityMode(&error, kSecurityModeNone));
  EXPECT_TRUE(config_.ValidateSecurityMode(&error, kSecurityModeRSN));
  EXPECT_FALSE(config_.ValidateSecurityMode(&error, "InvalidSecurityMode"));
}

TEST_F(ConfigTest, ValidatePassphrase) {
  brillo::ErrorPtr error;
  // Passpharse must contain between 8 and 63 characters.
  EXPECT_TRUE(config_.ValidatePassphrase(&error, std::string(8, 'c')));
  EXPECT_TRUE(config_.ValidatePassphrase(&error, std::string(63, 'c')));
  EXPECT_FALSE(config_.ValidatePassphrase(&error, std::string(7, 'c')));
  EXPECT_FALSE(config_.ValidatePassphrase(&error, std::string(64, 'c')));
}

TEST_F(ConfigTest, ValidateHwMode) {
  brillo::ErrorPtr error;
  EXPECT_TRUE(config_.ValidateHwMode(&error, kHwMode80211a));
  EXPECT_TRUE(config_.ValidateHwMode(&error, kHwMode80211b));
  EXPECT_TRUE(config_.ValidateHwMode(&error, kHwMode80211g));
  EXPECT_TRUE(config_.ValidateHwMode(&error, kHwMode80211n));
  EXPECT_TRUE(config_.ValidateHwMode(&error, kHwMode80211ac));
  EXPECT_FALSE(config_.ValidateSecurityMode(&error, "InvalidHwMode"));
}

TEST_F(ConfigTest, ValidateOperationMode) {
  brillo::ErrorPtr error;
  EXPECT_TRUE(config_.ValidateOperationMode(&error, kOperationModeServer));
  EXPECT_TRUE(config_.ValidateOperationMode(&error, kOperationModeBridge));
  EXPECT_FALSE(config_.ValidateOperationMode(&error, "InvalidMode"));
}

TEST_F(ConfigTest, ValidateChannel) {
  brillo::ErrorPtr error;
  EXPECT_TRUE(config_.ValidateChannel(&error, 1));
  EXPECT_TRUE(config_.ValidateChannel(&error, 13));
  EXPECT_TRUE(config_.ValidateChannel(&error, 34));
  EXPECT_TRUE(config_.ValidateChannel(&error, 165));
  EXPECT_FALSE(config_.ValidateChannel(&error, 0));
  EXPECT_FALSE(config_.ValidateChannel(&error, 14));
  EXPECT_FALSE(config_.ValidateChannel(&error, 33));
  EXPECT_FALSE(config_.ValidateChannel(&error, 166));
}

TEST_F(ConfigTest, NoSsid) {
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);

  std::string config_content;
  brillo::ErrorPtr error;
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error, IsConfigErrorStartingWith("SSID not specified"));
}

TEST_F(ConfigTest, NoInterface) {
  // Basic 80211.g configuration.
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);

  // No device available, fail to generate config file.
  brillo::ErrorPtr error;
  std::string config_content;
  EXPECT_CALL(manager_, GetAvailableDevice()).WillOnce(Return(nullptr));
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error, IsConfigErrorStartingWith("No device available"));
  Mock::VerifyAndClearExpectations(&manager_);

  // Device available, config file should be generated without any problem.
  scoped_refptr<MockDevice> device = new MockDevice(&manager_);
  device->SetPreferredApInterface(kInterface);
  brillo::ErrorPtr error1;
  EXPECT_CALL(manager_, GetAvailableDevice()).WillOnce(Return(device));
  EXPECT_TRUE(config_.GenerateConfigFile(&error1, &config_content));
  EXPECT_NE(std::string::npos, config_content.find(
                                   kExpected80211gConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211gConfigContent << "..within content...\n"
      << config_content;
  EXPECT_EQ(nullptr, error1.get());
  Mock::VerifyAndClearExpectations(&manager_);
}

TEST_F(ConfigTest, InvalidInterface) {
  // Basic 80211.g configuration.
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);

  // No device available, fail to generate config file.
  brillo::ErrorPtr error;
  std::string config_content;
  EXPECT_CALL(manager_, GetDeviceFromInterfaceName(kInterface))
      .WillOnce(Return(nullptr));
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error,
              IsConfigErrorStartingWith(
                  "Unable to find device for the specified interface"));
  Mock::VerifyAndClearExpectations(&manager_);
}

TEST_F(ConfigTest, BridgeMode) {
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);
  config_.SetOperationMode(kOperationModeBridge);

  // Bridge interface required for bridge mode.
  brillo::ErrorPtr error;
  std::string config_content;
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error,
              IsConfigErrorStartingWith("Bridge interface not specified"));

  // Set bridge interface, config file should be generated without error.
  config_.SetBridgeInterface(kBridgeInterface);
  // Setup mock device.
  SetupDevice(kInterface);
  brillo::ErrorPtr error1;
  std::string config_content1;
  EXPECT_TRUE(config_.GenerateConfigFile(&error1, &config_content1));
  EXPECT_NE(std::string::npos, config_content1.find(
                                   kExpected80211gBridgeConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211gBridgeConfigContent << "..within content...\n"
      << config_content1;
  EXPECT_EQ(nullptr, error1.get());
}

TEST_F(ConfigTest, 80211gConfig) {
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);

  // Setup mock device.
  SetupDevice(kInterface);

  std::string config_content;
  brillo::ErrorPtr error;
  EXPECT_TRUE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_NE(std::string::npos, config_content.find(
                                   kExpected80211gConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211gConfigContent << "..within content...\n"
      << config_content;
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(ConfigTest, 80211gConfigWithControlInterface) {
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);
  config_.set_control_interface(kControlInterfacePath);

  // Setup mock device.
  SetupDevice(kInterface);

  std::string config_content;
  brillo::ErrorPtr error;
  EXPECT_TRUE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_NE(std::string::npos, config_content.find(
                                   kExpected80211gCtrlIfaceConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211gCtrlIfaceConfigContent << "..within content...\n"
      << config_content;
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(ConfigTest, 80211nConfig) {
  config_.SetSsid(kSsid);
  config_.SetHwMode(kHwMode80211n);
  config_.SetInterfaceName(kInterface);

  // Setup mock device.
  SetupDevice(kInterface);

  // 5GHz channel.
  config_.SetChannel(k5GHzChannel);
  std::string ghz5_config_content;
  brillo::ErrorPtr error;
  std::string ht_capab_5ghz(k5GHzHTCapab);
  EXPECT_CALL(*device_.get(), GetHTCapability(k5GHzChannel, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(ht_capab_5ghz), Return(true)));
  EXPECT_TRUE(config_.GenerateConfigFile(&error, &ghz5_config_content));
  EXPECT_NE(std::string::npos, ghz5_config_content.find(
                                   kExpected80211n5GHzConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211n5GHzConfigContent << "..within content...\n"
      << ghz5_config_content;
  EXPECT_EQ(nullptr, error.get());
  Mock::VerifyAndClearExpectations(device_.get());

  // 2.4GHz channel.
  config_.SetChannel(k24GHzChannel);
  std::string ghz24_config_content;
  brillo::ErrorPtr error1;
  std::string ht_capab_24ghz(k24GHzHTCapab);
  EXPECT_CALL(*device_.get(), GetHTCapability(k24GHzChannel, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(ht_capab_24ghz), Return(true)));
  EXPECT_TRUE(config_.GenerateConfigFile(&error1, &ghz24_config_content));
  EXPECT_NE(std::string::npos, ghz24_config_content.find(
                                   kExpected80211n24GHzConfigContent))
      << "Expected to find the following config...\n"
      << kExpected80211n24GHzConfigContent << "..within content...\n"
      << ghz24_config_content;
  EXPECT_EQ(nullptr, error.get());
  Mock::VerifyAndClearExpectations(device_.get());
}

TEST_F(ConfigTest, RsnConfig) {
  config_.SetSsid(kSsid);
  config_.SetChannel(k24GHzChannel);
  config_.SetHwMode(kHwMode80211g);
  config_.SetInterfaceName(kInterface);
  config_.SetSecurityMode(kSecurityModeRSN);

  // Setup mock device.
  SetupDevice(kInterface);

  // Failed due to no passphrase specified.
  std::string config_content;
  brillo::ErrorPtr error;
  EXPECT_FALSE(config_.GenerateConfigFile(&error, &config_content));
  EXPECT_THAT(error, IsConfigErrorStartingWith(
      base::StringPrintf("Passphrase not set for security mode: %s",
                         kSecurityModeRSN)));

  brillo::ErrorPtr error1;
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
