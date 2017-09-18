// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_set>

#include <base/strings/string_util.h>
#include <components/policy/core/common/registry_dict.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "authpolicy/policy/device_policy_encoder.h"
#include "authpolicy/policy/policy_encoder_test_base.h"
#include "bindings/chrome_device_policy.pb.h"
#include "bindings/policy_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Converts a repeated string field to a vector.
std::vector<std::string> ToVector(
    const google::protobuf::RepeatedPtrField<std::string>& repeated_field) {
  return std::vector<std::string>(repeated_field.begin(), repeated_field.end());
}

// Converts a repeated int field to a vector.
std::vector<int> ToVector(
    const google::protobuf::RepeatedField<int>& repeated_field) {
  return std::vector<int>(repeated_field.begin(), repeated_field.end());
}

}  // namespace

// Checks whether all device policies are properly encoded from RegistryDict
// into em::ChromeDeviceSettingsProto. Makes sure no device policy is missing.
class DevicePolicyEncoderTest
    : public PolicyEncoderTestBase<em::ChromeDeviceSettingsProto> {
 public:
  DevicePolicyEncoderTest() {}
  ~DevicePolicyEncoderTest() override {}

 protected:
  void EncodeDict(em::ChromeDeviceSettingsProto* policy,
                  const RegistryDict* dict) override {
    DevicePolicyEncoder encoder(dict);
    *policy = em::ChromeDeviceSettingsProto();
    encoder.EncodePolicy(policy);
  }

  void MarkHandled(const char* key) override {
    handled_policy_keys_.insert(key);
  }

  // Returns a vector of all policy keys that were not encoded or otherwise
  // marked handled (e.g. unsupported policies).
  std::vector<std::string> GetUnhandledPolicyKeys() const {
    std::vector<std::string> unhandled_policy_keys;
    for (const char** key = kDevicePolicyKeys; *key; ++key) {
      if (handled_policy_keys_.find(*key) == handled_policy_keys_.end())
        unhandled_policy_keys.push_back(*key);
    }
    return unhandled_policy_keys;
  }

 private:
  // Keeps track of handled device policies. Used to detect device policies that
  // device_policy_encoder forgets to encode.
  std::unordered_set<std::string> handled_policy_keys_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyEncoderTest);
};

TEST_F(DevicePolicyEncoderTest, TestEncoding) {
  // Note that kStringList can't be constexpr, so we put them all here.
  const bool kBool = true;
  const int kInt = 123;
  const std::string kString = "val1";
  const std::vector<std::string> kStringList = {"val1", "val2", "val3"};

  em::ChromeDeviceSettingsProto policy;

  //
  // Login policies.
  //

  EncodeBoolean(&policy, key::kDeviceGuestModeEnabled, kBool);
  EXPECT_EQ(kBool, policy.guest_mode_enabled().guest_mode_enabled());

  EncodeBoolean(&policy, key::kDeviceRebootOnShutdown, kBool);
  EXPECT_EQ(kBool, policy.reboot_on_shutdown().reboot_on_shutdown());

  EncodeBoolean(&policy, key::kDeviceShowUserNamesOnSignin, kBool);
  EXPECT_EQ(kBool, policy.show_user_names().show_user_names());

  EncodeBoolean(&policy, key::kDeviceAllowNewUsers, kBool);
  EXPECT_EQ(kBool, policy.allow_new_users().allow_new_users());

  EncodeStringList(&policy, key::kDeviceUserWhitelist, kStringList);
  EXPECT_EQ(kStringList, ToVector(policy.user_whitelist().user_whitelist()));

  EncodeBoolean(&policy, key::kDeviceEphemeralUsersEnabled, kBool);
  EXPECT_EQ(kBool, policy.ephemeral_users_enabled().ephemeral_users_enabled());

  EncodeInteger(&policy,
                key::kDeviceEcryptfsMigrationStrategy,
                em::DeviceEcryptfsMigrationStrategyProto::DISALLOW_ARC);
  EXPECT_EQ(em::DeviceEcryptfsMigrationStrategyProto::DISALLOW_ARC,
            policy.device_ecryptfs_migration_strategy().migration_strategy());

  // Unsupported, see device_policy_encoder.cc for explanation, simply mark
  // handled.
  MarkHandled(key::kDeviceLocalAccounts);
  EncodeString(&policy, key::kDeviceLocalAccountAutoLoginId, kString);
  EXPECT_EQ(kString, policy.device_local_accounts().auto_login_id());
  EncodeInteger(&policy, key::kDeviceLocalAccountAutoLoginDelay, kInt);
  EXPECT_EQ(kInt, policy.device_local_accounts().auto_login_delay());
  EncodeBoolean(
      &policy, key::kDeviceLocalAccountAutoLoginBailoutEnabled, kBool);
  EXPECT_EQ(kBool, policy.device_local_accounts().enable_auto_login_bailout());
  EncodeBoolean(
      &policy, key::kDeviceLocalAccountPromptForNetworkWhenOffline, kBool);
  EXPECT_EQ(kBool,
            policy.device_local_accounts().prompt_for_network_when_offline());

  EncodeBoolean(&policy, key::kSupervisedUsersEnabled, kBool);
  EXPECT_EQ(kBool,
            policy.supervised_users_settings().supervised_users_enabled());

  EncodeBoolean(&policy, key::kDeviceTransferSAMLCookies, kBool);
  EXPECT_EQ(kBool, policy.saml_settings().transfer_saml_cookies());

  // The encoder of this policy converts ints to LoginBehavior enums.
  EncodeInteger(&policy,
                key::kLoginAuthenticationBehavior,
                em::LoginAuthenticationBehaviorProto::SAML_INTERSTITIAL);
  EXPECT_EQ(
      em::LoginAuthenticationBehaviorProto::SAML_INTERSTITIAL,
      policy.login_authentication_behavior().login_authentication_behavior());

  EncodeBoolean(&policy, key::kDeviceAllowBluetooth, kBool);
  EXPECT_EQ(kBool, policy.allow_bluetooth().allow_bluetooth());
  EncodeStringList(&policy, key::kLoginVideoCaptureAllowedUrls, kStringList);
  EXPECT_EQ(kStringList,
            ToVector(policy.login_video_capture_allowed_urls().urls()));
  EncodeStringList(&policy, key::kDeviceLoginScreenAppInstallList, kStringList);
  EXPECT_EQ(kStringList,
            ToVector(policy.device_login_screen_app_install_list()
                         .device_login_screen_app_install_list()));
  EncodeStringList(&policy, key::kDeviceLoginScreenLocales, kStringList);
  EXPECT_EQ(kStringList,
            ToVector(policy.login_screen_locales().login_screen_locales()));
  EncodeStringList(&policy, key::kDeviceLoginScreenInputMethods, kStringList);
  EXPECT_EQ(
      kStringList,
      ToVector(
          policy.login_screen_input_methods().login_screen_input_methods()));

  //
  // Network policies.
  //

  EncodeBoolean(&policy, key::kDeviceDataRoamingEnabled, kBool);
  EXPECT_EQ(kBool, policy.data_roaming_enabled().data_roaming_enabled());

  // The encoder of this policy converts a JSON string to separate values.
  EncodeString(&policy,
               key::kNetworkThrottlingEnabled,
               "{\"enabled\":true,"
               " \"upload_rate_kbits\":128,"
               " \"download_rate_kbits\":256}");
  EXPECT_EQ(true, policy.network_throttling().enabled());
  EXPECT_EQ(128, policy.network_throttling().upload_rate_kbits());
  EXPECT_EQ(256, policy.network_throttling().download_rate_kbits());

  EncodeString(&policy, key::kDeviceOpenNetworkConfiguration, kString);
  EXPECT_EQ(kString,
            policy.open_network_configuration().open_network_configuration());

  //
  // Reporting policies.
  //

  EncodeBoolean(&policy, key::kReportDeviceVersionInfo, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_version_info());
  EncodeBoolean(&policy, key::kReportDeviceActivityTimes, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_activity_times());
  EncodeBoolean(&policy, key::kReportDeviceBootMode, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_boot_mode());
  EncodeBoolean(&policy, key::kReportDeviceLocation, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_location());
  EncodeBoolean(&policy, key::kReportDeviceNetworkInterfaces, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_network_interfaces());
  EncodeBoolean(&policy, key::kReportDeviceUsers, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_users());
  EncodeBoolean(&policy, key::kReportDeviceHardwareStatus, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_hardware_status());
  EncodeBoolean(&policy, key::kReportDeviceSessionStatus, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().report_session_status());
  EncodeBoolean(&policy, key::kReportUploadFrequency, kBool);
  EXPECT_EQ(kBool, policy.device_reporting().device_status_frequency());

  EncodeBoolean(&policy, key::kHeartbeatEnabled, kBool);
  EXPECT_EQ(kBool, policy.device_heartbeat_settings().heartbeat_enabled());
  EncodeInteger(&policy, key::kHeartbeatFrequency, kInt);
  EXPECT_EQ(kInt, policy.device_heartbeat_settings().heartbeat_frequency());

  EncodeBoolean(&policy, key::kLogUploadEnabled, kBool);
  EXPECT_EQ(kBool,
            policy.device_log_upload_settings().system_log_upload_enabled());

  //
  // Auto update policies.
  //

  EncodeString(&policy, key::kChromeOsReleaseChannel, kString);
  EXPECT_EQ(kString, policy.release_channel().release_channel());
  EncodeBoolean(&policy, key::kChromeOsReleaseChannelDelegated, kBool);
  EXPECT_EQ(kBool, policy.release_channel().release_channel_delegated());

  EncodeBoolean(&policy, key::kDeviceAutoUpdateDisabled, kBool);
  EXPECT_EQ(kBool, policy.auto_update_settings().update_disabled());
  EncodeString(&policy, key::kDeviceTargetVersionPrefix, kString);
  EXPECT_EQ(kString, policy.auto_update_settings().target_version_prefix());

  EncodeInteger(&policy, key::kDeviceUpdateScatterFactor, kInt);
  EXPECT_EQ(kInt, policy.auto_update_settings().scatter_factor_in_seconds());
  // The encoder of this policy converts connection type strings to enums.
  std::vector<std::string> str_types;
  std::vector<int> enum_types;
  for (size_t n = 0; n < kConnectionTypesSize; ++n) {
    str_types.push_back(kConnectionTypes[n].first);
    enum_types.push_back(kConnectionTypes[n].second);
  }
  EncodeStringList(
      &policy, key::kDeviceUpdateAllowedConnectionTypes, str_types);
  EXPECT_EQ(enum_types,
            ToVector(policy.auto_update_settings().allowed_connection_types()));
  EncodeBoolean(&policy, key::kDeviceUpdateHttpDownloadsEnabled, kBool);
  EXPECT_EQ(kBool, policy.auto_update_settings().http_downloads_enabled());
  EncodeBoolean(&policy, key::kRebootAfterUpdate, kBool);
  EXPECT_EQ(kBool, policy.auto_update_settings().reboot_after_update());
  EncodeBoolean(&policy, key::kDeviceAutoUpdateP2PEnabled, kBool);
  EXPECT_EQ(kBool, policy.auto_update_settings().p2p_enabled());

  EncodeBoolean(&policy, key::kAllowKioskAppControlChromeVersion, kBool);
  EXPECT_EQ(kBool,
            policy.allow_kiosk_app_control_chrome_version()
                .allow_kiosk_app_control_chrome_version());

  //
  // Accessibility policies.
  //

  EncodeBoolean(
      &policy, key::kDeviceLoginScreenDefaultLargeCursorEnabled, kBool);
  EXPECT_EQ(kBool,
            policy.accessibility_settings()
                .login_screen_default_large_cursor_enabled());
  EncodeBoolean(
      &policy, key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled, kBool);
  EXPECT_EQ(kBool,
            policy.accessibility_settings()
                .login_screen_default_spoken_feedback_enabled());
  EncodeBoolean(
      &policy, key::kDeviceLoginScreenDefaultHighContrastEnabled, kBool);
  EXPECT_EQ(kBool,
            policy.accessibility_settings()
                .login_screen_default_high_contrast_enabled());
  // The encoder of this policy converts ints to ScreenMagnifierType enums.
  EncodeInteger(&policy,
                key::kDeviceLoginScreenDefaultScreenMagnifierType,
                em::AccessibilitySettingsProto::SCREEN_MAGNIFIER_TYPE_FULL);
  EXPECT_EQ(em::AccessibilitySettingsProto::SCREEN_MAGNIFIER_TYPE_FULL,
            policy.accessibility_settings()
                .login_screen_default_screen_magnifier_type());
  EncodeBoolean(
      &policy, key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled, kBool);
  EXPECT_EQ(kBool,
            policy.accessibility_settings()
                .login_screen_default_virtual_keyboard_enabled());

  //
  // Generic policies.
  //

  EncodeInteger(&policy, key::kDevicePolicyRefreshRate, kInt);
  EXPECT_EQ(kInt,
            policy.device_policy_refresh_rate().device_policy_refresh_rate());

  EncodeBoolean(&policy, key::kDeviceMetricsReportingEnabled, kBool);
  EXPECT_EQ(kBool, policy.metrics_enabled().metrics_enabled());

  EncodeString(&policy, key::kSystemTimezone, kString);
  EXPECT_EQ(kString, policy.system_timezone().timezone());
  // The encoder of this policy converts ints to AutomaticTimezoneDetectionType
  // enums.
  EncodeInteger(&policy,
                key::kSystemTimezoneAutomaticDetection,
                em::SystemTimezoneProto::IP_ONLY);
  EXPECT_EQ(em::SystemTimezoneProto::IP_ONLY,
            policy.system_timezone().timezone_detection_type());

  EncodeBoolean(&policy, key::kSystemUse24HourClock, kBool);
  EXPECT_EQ(kBool, policy.use_24hour_clock().use_24hour_clock());

  EncodeBoolean(
      &policy, key::kDeviceAllowRedeemChromeOsRegistrationOffers, kBool);
  EXPECT_EQ(kBool, policy.allow_redeem_offers().allow_redeem_offers());

  EncodeInteger(&policy, key::kUptimeLimit, kInt);
  EXPECT_EQ(kInt, policy.uptime_limit().uptime_limit());

  EncodeStringList(&policy, key::kDeviceStartUpFlags, kStringList);
  EXPECT_EQ(kStringList, ToVector(policy.start_up_flags().flags()));

  EncodeString(&policy, key::kDeviceVariationsRestrictParameter, kString);
  EXPECT_EQ(kString, policy.variations_parameter().parameter());

  EncodeBoolean(&policy, key::kAttestationEnabledForDevice, kBool);
  EXPECT_EQ(kBool, policy.attestation_settings().attestation_enabled());
  EncodeBoolean(&policy, key::kAttestationForContentProtectionEnabled, kBool);
  EXPECT_EQ(kBool, policy.attestation_settings().content_protection_enabled());

  EncodeString(&policy, key::kDeviceLoginScreenPowerManagement, kString);
  EXPECT_EQ(
      kString,
      policy.login_screen_power_management().login_screen_power_management());

  EncodeBoolean(&policy, key::kDeviceBlockDevmode, kBool);
  EXPECT_EQ(kBool, policy.system_settings().block_devmode());

  EncodeInteger(&policy, key::kExtensionCacheSize, kInt);
  EXPECT_EQ(kInt, policy.extension_cache_size().extension_cache_size());

  EncodeString(&policy, key::kDeviceLoginScreenDomainAutoComplete, kString);
  EXPECT_EQ(kString,
            policy.login_screen_domain_auto_complete()
                .login_screen_domain_auto_complete());

  // The encoder of this policy converts ints to Rotation enums.
  EncodeInteger(&policy,
                key::kDisplayRotationDefault,
                em::DisplayRotationDefaultProto::ROTATE_180);
  EXPECT_EQ(em::DisplayRotationDefaultProto::ROTATE_180,
            policy.display_rotation_default().display_rotation_default());

  // The encoder of this policy converts a JSON string to separate values.
  EncodeStringList(&policy,
                   key::kUsbDetachableWhitelist,
                   {"{\"vendor_id\":123, \"product_id\":234}",
                    "{\"vendor_id\":345, \"product_id\":456}"});
  const auto& whitelist_proto = policy.usb_detachable_whitelist();
  EXPECT_EQ(123, whitelist_proto.id().Get(0).vendor_id());
  EXPECT_EQ(234, whitelist_proto.id().Get(0).product_id());
  EXPECT_EQ(345, whitelist_proto.id().Get(1).vendor_id());
  EXPECT_EQ(456, whitelist_proto.id().Get(1).product_id());

  EncodeBoolean(&policy, key::kDeviceQuirksDownloadEnabled, kBool);
  EXPECT_EQ(kBool, policy.quirks_download_enabled().quirks_download_enabled());

  EncodeString(&policy, key::kDeviceWallpaperImage, kString);
  EXPECT_EQ(kString, policy.device_wallpaper_image().device_wallpaper_image());

  EncodeInteger(&policy,
                key::kDeviceSecondFactorAuthentication,
                em::DeviceSecondFactorAuthenticationProto::U2F);
  EXPECT_EQ(em::DeviceSecondFactorAuthenticationProto::U2F,
            policy.device_second_factor_authentication().mode());

  EncodeString(&policy,
               key::kDeviceOffHours,
               R"!!!(
               {
                 "intervals":
                 [
                   {
                     "start": {
                       "day_of_week": "MONDAY",
                       "time": 12840000
                     },
                     "end": {
                       "day_of_week": "MONDAY",
                       "time": 21720000
                     }
                   },
                   {
                     "start": {
                       "day_of_week": "FRIDAY",
                       "time": 38640000
                     },
                     "end": {
                       "day_of_week": "FRIDAY",
                       "time": 57600000
                     }
                   }
                 ],
                 "timezone": "GMT",
                 "ignored_policies": ["policy1", "policy2"]
               })!!!");
  const auto& device_off_hours_proto = policy.device_off_hours();
  EXPECT_EQ(2, device_off_hours_proto.intervals_size());
  const auto& interval1 = device_off_hours_proto.intervals().Get(0);
  const auto& interval2 = device_off_hours_proto.intervals().Get(1);
  EXPECT_EQ(em::WeeklyTimeProto::MONDAY, interval1.start().day_of_week());
  EXPECT_EQ(em::WeeklyTimeProto::MONDAY, interval1.end().day_of_week());
  EXPECT_EQ(12840000, interval1.start().time());
  EXPECT_EQ(21720000, interval1.end().time());
  EXPECT_EQ(em::WeeklyTimeProto::FRIDAY, interval2.start().day_of_week());
  EXPECT_EQ(em::WeeklyTimeProto::FRIDAY, interval2.end().day_of_week());
  EXPECT_EQ(38640000, interval2.start().time());
  EXPECT_EQ(57600000, interval2.end().time());
  EXPECT_EQ("GMT", device_off_hours_proto.timezone());
  EXPECT_EQ("policy1", device_off_hours_proto.ignored_policies().Get(0));
  EXPECT_EQ("policy2", device_off_hours_proto.ignored_policies().Get(1));

  EncodeString(&policy, key::kCastReceiverName, kString);
  EXPECT_EQ(kString, policy.cast_receiver_name().name());

  // The encoder of this policy converts ints to AccessMode enums.
  EncodeString(&policy, key::kDeviceNativePrinters, kString);
  EXPECT_EQ(kString, policy.native_device_printers().external_policy());
  EncodeInteger(&policy,
                key::kDeviceNativePrintersAccessMode,
                em::DeviceNativePrintersAccessModeProto::ACCESS_MODE_WHITELIST);
  EXPECT_EQ(em::DeviceNativePrintersAccessModeProto::ACCESS_MODE_WHITELIST,
            policy.native_device_printers_access_mode().access_mode());
  EncodeStringList(&policy, key::kDeviceNativePrintersWhitelist, kStringList);
  EXPECT_EQ(kStringList,
            ToVector(policy.native_device_printers_whitelist().whitelist()));
  EncodeStringList(&policy, key::kDeviceNativePrintersBlacklist, kStringList);
  EXPECT_EQ(kStringList,
            ToVector(policy.native_device_printers_blacklist().blacklist()));

  //
  // Check whether all device policies have been handled.
  //

  std::vector<std::string> unhandled_policy_keys = GetUnhandledPolicyKeys();
  EXPECT_TRUE(unhandled_policy_keys.empty())
      << "Unhandled policy detected.\n"
      << "Please handle the following policies in "
      << "device_policy_encoder.cc and device_policy_encoder_unittest.cc:\n"
      << "  " << base::JoinString(unhandled_policy_keys, "\n  ");
}

}  // namespace policy
