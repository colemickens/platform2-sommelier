// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/macros.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "authpolicy/policy/policy_encoder_helper.h"
#include "authpolicy/policy/preg_policy_encoder.h"
#include "authpolicy/policy/preg_policy_writer.h"
#include "bindings/chrome_device_policy.pb.h"
#include "bindings/cloud_policy.pb.h"
#include "bindings/policy_constants.h"

namespace em = enterprise_management;

namespace {

const char kPreg1Filename[] = "registry1.pol";
const char kPreg2Filename[] = "registry2.pol";

// Some constants for policy testing.
const bool kPolicyBool = true;
const int kPolicyInt = 321;
const bool kOtherPolicyBool = false;
const int kOtherPolicyInt = 234;

const char kHomepageUrl[] = "www.example.com";
const char kAltHomepageUrl[] = "www.exemplum.com";
const char kTimezone[] = "Sankt Aldegund Central Time";
const char kAltTimezone[] = "Bremm Prehistoric Time";

}  // namespace

namespace policy {

// Tests basic preg-to-policy encoding and the promises
// ParsePRegFilesIntoUserPolicy and ParsePRegFilesIntoDevicePolicy make.
class PregPolicyEncoderTest : public ::testing::Test {
 public:
  PregPolicyEncoderTest() = default;

  void SetUp() override {
    // Create temp directory for policy files.
    CHECK(base::CreateNewTempDirectory("" /* prefix (ignored) */, &base_path_));

    preg_1_path_ = base_path_.Append(kPreg1Filename);
    preg_2_path_ = base_path_.Append(kPreg2Filename);
  }

  void TearDown() override {
    // Don't not leave no mess behind.
    base::DeleteFile(base_path_, true /* recursive */);
  }

 protected:
  // Whether policies from the first or the second file win.
  enum WhoWins { FIRST_WINS, SECOND_WINS };

  // Writes two files with policies, encodes them and tests whether policies
  // from the first of the second file 'won' (prevailed). |level1| and |level2|
  // are the policy levels for the two files.
  void TestUserPolicyFileOverride(PolicyLevel level1,
                                  PolicyLevel level2,
                                  WhoWins who_wins) {
    // Write file 1 with some interesting data. Set policy level to level1.
    PRegPolicyWriter writer1(GetRegistryKey());
    writer1.AppendBoolean(key::kSearchSuggestEnabled, kOtherPolicyBool, level1);
    writer1.AppendInteger(key::kPolicyRefreshRate, kPolicyInt, level1);
    writer1.AppendString(key::kHomepageLocation, kHomepageUrl, level1);
    const std::vector<std::string> apps1 = {"App1", "App2", "App3"};
    writer1.AppendStringList(key::kPinnedLauncherApps, apps1, level1);
    writer1.WriteToFile(preg_1_path_);

    // Write file 2 with the same policies, but different values. Set policy
    // level to level2.
    PRegPolicyWriter writer2(GetRegistryKey());
    writer2.AppendBoolean(key::kSearchSuggestEnabled, kPolicyBool, level2);
    writer2.AppendInteger(key::kPolicyRefreshRate, kOtherPolicyInt, level2);
    writer2.AppendString(key::kHomepageLocation, kAltHomepageUrl, level2);
    const std::vector<std::string> apps2 = {"App4", "App5"};
    writer2.AppendStringList(key::kPinnedLauncherApps, apps2, level2);
    writer2.WriteToFile(preg_2_path_);

    // Encode to policy.
    em::CloudPolicySettings policy;
    EXPECT_TRUE(ParsePRegFilesIntoUserPolicy(
        {preg_1_path_, preg_2_path_}, &policy, false /* log_policy_values */));

    bool win_bool = (who_wins == FIRST_WINS ? kOtherPolicyBool : kPolicyBool);
    int win_int = (who_wins == FIRST_WINS ? kPolicyInt : kOtherPolicyInt);
    const std::string& win_string =
        (who_wins == FIRST_WINS ? kHomepageUrl : kAltHomepageUrl);
    const auto& win_string_list = (who_wins == FIRST_WINS ? apps1 : apps2);
    PolicyLevel win_level = (who_wins == FIRST_WINS ? level1 : level2);
    em::PolicyOptions::PolicyMode win_mode =
        win_level == POLICY_LEVEL_MANDATORY ? em::PolicyOptions::MANDATORY
                                            : em::PolicyOptions::RECOMMENDED;

    // Check that the expected values prevail.
    EXPECT_TRUE(policy.searchsuggestenabled().has_policy_options());
    EXPECT_EQ(win_mode, policy.searchsuggestenabled().policy_options().mode());
    EXPECT_TRUE(policy.searchsuggestenabled().has_value());
    EXPECT_EQ(win_bool, policy.searchsuggestenabled().value());

    EXPECT_TRUE(policy.policyrefreshrate().has_policy_options());
    EXPECT_EQ(win_mode, policy.policyrefreshrate().policy_options().mode());
    EXPECT_TRUE(policy.policyrefreshrate().has_value());
    EXPECT_EQ(win_int, policy.policyrefreshrate().value());

    EXPECT_TRUE(policy.homepagelocation().has_policy_options());
    EXPECT_EQ(win_mode, policy.homepagelocation().policy_options().mode());
    EXPECT_TRUE(policy.homepagelocation().has_value());
    EXPECT_EQ(win_string, policy.homepagelocation().value());

    EXPECT_TRUE(policy.homepagelocation().has_policy_options());
    EXPECT_EQ(win_mode, policy.pinnedlauncherapps().policy_options().mode());
    EXPECT_TRUE(policy.pinnedlauncherapps().has_value());
    const em::StringList& apps_proto = policy.pinnedlauncherapps().value();
    EXPECT_EQ(apps_proto.entries_size(),
              static_cast<int>(win_string_list.size()));
    for (int n = 0; n < apps_proto.entries_size(); ++n)
      EXPECT_EQ(apps_proto.entries(n), win_string_list.at(n));
  }

  base::FilePath base_path_;
  base::FilePath preg_1_path_;
  base::FilePath preg_2_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PregPolicyEncoderTest);
};

// Encodes user policies of different types.
TEST_F(PregPolicyEncoderTest, UserPolicyEncodingWorks) {
  // Create a preg file with some interesting data.
  PRegPolicyWriter writer(GetRegistryKey());
  writer.AppendBoolean(key::kSearchSuggestEnabled, kPolicyBool);
  writer.AppendInteger(key::kPolicyRefreshRate, kPolicyInt);
  writer.AppendString(key::kHomepageLocation, kHomepageUrl);
  const std::vector<std::string> apps = {"App1", "App2"};
  writer.AppendStringList(key::kPinnedLauncherApps, apps);
  writer.WriteToFile(preg_1_path_);

  // Encode preg file into policy.
  em::CloudPolicySettings policy;
  EXPECT_TRUE(ParsePRegFilesIntoUserPolicy(
      {preg_1_path_}, &policy, false /* log_policy_values */));

  // Check that policy has the same values as we wrote to the file.
  EXPECT_EQ(kPolicyBool, policy.searchsuggestenabled().value());
  EXPECT_EQ(kPolicyInt, policy.policyrefreshrate().value());
  EXPECT_EQ(kHomepageUrl, policy.homepagelocation().value());
  const em::StringList& apps_proto = policy.pinnedlauncherapps().value();
  EXPECT_EQ(apps_proto.entries_size(), static_cast<int>(apps.size()));
  for (int n = 0; n < apps_proto.entries_size(); ++n)
    EXPECT_EQ(apps_proto.entries(n), apps.at(n));
}

// Checks that a user GPO later in the list overrides prior GPOs for recommended
// policies.
TEST_F(PregPolicyEncoderTest, UserPolicyRecommendedOverridesRecommended) {
  TestUserPolicyFileOverride(
      POLICY_LEVEL_RECOMMENDED, POLICY_LEVEL_RECOMMENDED, SECOND_WINS);
}

// Checks that a user GPO later in the list overrides prior GPOs for mandatory
// policies.
TEST_F(PregPolicyEncoderTest, UserPolicyMandatoryOverridesMandatory) {
  TestUserPolicyFileOverride(
      POLICY_LEVEL_MANDATORY, POLICY_LEVEL_MANDATORY, SECOND_WINS);
}

// Verifies that a mandatory policy is not overridden by a recommended policy.
TEST_F(PregPolicyEncoderTest, UserPolicyRecommendedDoesNotOverrideMandatory) {
  TestUserPolicyFileOverride(
      POLICY_LEVEL_MANDATORY, POLICY_LEVEL_RECOMMENDED, FIRST_WINS);
}

// Verifies that a mandatory policy overrides a recommended policy.
TEST_F(PregPolicyEncoderTest, UserPolicyMandatoryOverridesRecommended) {
  TestUserPolicyFileOverride(
      POLICY_LEVEL_RECOMMENDED, POLICY_LEVEL_MANDATORY, SECOND_WINS);
}

// Encodes device policies of different types.
TEST_F(PregPolicyEncoderTest, DevicePolicyEncodingWorks) {
  // Create a preg file with some interesting data.
  PRegPolicyWriter writer(GetRegistryKey());
  writer.AppendBoolean(key::kDeviceGuestModeEnabled, kPolicyBool);
  writer.AppendInteger(key::kDeviceLocalAccountAutoLoginDelay, kPolicyInt);
  writer.AppendString(key::kSystemTimezone, kTimezone);
  const std::vector<std::string> flags = {"flag1", "flag2"};
  writer.AppendStringList(key::kDeviceStartUpFlags, flags);
  writer.WriteToFile(preg_1_path_);

  // Encode preg file into policy.
  em::ChromeDeviceSettingsProto policy;
  EXPECT_TRUE(ParsePRegFilesIntoDevicePolicy(
      {preg_1_path_}, &policy, false /* log_policy_values */));

  // Check that policy has the same values as we wrote to the file.
  EXPECT_EQ(kPolicyBool, policy.guest_mode_enabled().guest_mode_enabled());
  EXPECT_EQ(kPolicyInt, policy.device_local_accounts().auto_login_delay());
  EXPECT_EQ(kTimezone, policy.system_timezone().timezone());
  const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
  EXPECT_EQ(flags_proto.flags_size(), static_cast<int>(flags.size()));
  for (int n = 0; n < flags_proto.flags_size(); ++n)
    EXPECT_EQ(flags_proto.flags(n), flags.at(n));
}

// Checks that a device GPO later in the list overrides prior GPOs.
TEST_F(PregPolicyEncoderTest, DevicePolicyFileOverride) {
  // Write file 1 with some interesting data. Note that device policy doesn't
  // support mandatory/recommended policies.
  PRegPolicyWriter writer1(GetRegistryKey());
  writer1.AppendBoolean(key::kDeviceGuestModeEnabled, kOtherPolicyBool);
  writer1.AppendInteger(key::kDeviceLocalAccountAutoLoginDelay, kPolicyInt);
  writer1.AppendString(key::kSystemTimezone, kTimezone);
  const std::vector<std::string> flags1 = {"flag1", "flag2", "flag3"};
  writer1.AppendStringList(key::kDeviceStartUpFlags, flags1);
  writer1.WriteToFile(preg_1_path_);

  // Write file 2 with the same policies, but different values.
  PRegPolicyWriter writer2(GetRegistryKey());
  writer2.AppendBoolean(key::kDeviceGuestModeEnabled, kPolicyBool);
  writer2.AppendInteger(key::kDeviceLocalAccountAutoLoginDelay,
                        kOtherPolicyInt);
  writer2.AppendString(key::kSystemTimezone, kAltTimezone);
  const std::vector<std::string> flags2 = {"flag4", "flag5"};
  writer2.AppendStringList(key::kDeviceStartUpFlags, flags2);
  writer2.WriteToFile(preg_2_path_);

  // Encode to policy.
  em::ChromeDeviceSettingsProto policy;
  EXPECT_TRUE(ParsePRegFilesIntoDevicePolicy(
      {preg_1_path_, preg_2_path_}, &policy, false /* log_policy_values */));

  // Check that the values from file 2 prevailed.
  EXPECT_EQ(kPolicyBool, policy.guest_mode_enabled().guest_mode_enabled());
  EXPECT_EQ(kOtherPolicyInt, policy.device_local_accounts().auto_login_delay());
  EXPECT_EQ(kAltTimezone, policy.system_timezone().timezone());
  const em::StartUpFlagsProto& flags_proto = policy.start_up_flags();
  EXPECT_EQ(flags_proto.flags_size(), static_cast<int>(flags2.size()));
  for (int n = 0; n < flags_proto.flags_size(); ++n)
    EXPECT_EQ(flags_proto.flags(n), flags2.at(n));
}

}  // namespace policy
