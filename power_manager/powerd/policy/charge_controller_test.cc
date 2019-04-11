// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/charge_controller.h"

#include <map>
#include <string>
#include <vector>

#include <base/strings/string_split.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/system/charge_controller_helper_stub.h"
#include "power_manager/proto_bindings/policy.pb.h"

namespace power_manager {
namespace policy {

namespace {

void MakePeakShiftDayConfig(
    PowerManagementPolicy::WeekDay week_day,
    const std::string& config_str,
    PowerManagementPolicy::PeakShiftDayConfig* config_proto) {
  DCHECK(config_proto);

  config_proto->set_day(week_day);

  std::vector<std::string> splitted = base::SplitString(
      config_str, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(splitted.size(), 6);

  int value;
  ASSERT_TRUE(base::StringToInt(splitted[0], &value));
  config_proto->mutable_start_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(splitted[1], &value));
  config_proto->mutable_start_time()->set_minute(value);
  ASSERT_TRUE(base::StringToInt(splitted[2], &value));
  config_proto->mutable_end_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(splitted[3], &value));
  config_proto->mutable_end_time()->set_minute(value);
  ASSERT_TRUE(base::StringToInt(splitted[4], &value));
  config_proto->mutable_charge_start_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(splitted[5], &value));
  config_proto->mutable_charge_start_time()->set_minute(value);
}

void MakeAdvancedBatteryChargeModeDayConfig(
    PowerManagementPolicy::WeekDay week_day,
    const std::string& config_str,
    PowerManagementPolicy::AdvancedBatteryChargeModeDayConfig* config_proto) {
  DCHECK(config_proto);

  config_proto->set_day(week_day);

  std::vector<std::string> splitted = base::SplitString(
      config_str, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(splitted.size(), 4);

  int value;
  ASSERT_TRUE(base::StringToInt(splitted[0], &value));
  config_proto->mutable_charge_start_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(splitted[1], &value));
  config_proto->mutable_charge_start_time()->set_minute(value);
  ASSERT_TRUE(base::StringToInt(splitted[2], &value));
  config_proto->mutable_charge_end_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(splitted[3], &value));
  config_proto->mutable_charge_end_time()->set_minute(value);
}

class ChargeControllerTest : public ::testing::Test {
 public:
  ChargeControllerTest() { controller_.Init(&helper_); }

 protected:
  system::ChargeControllerHelperStub helper_;
  ChargeController controller_;
  PowerManagementPolicy policy_;
};

}  // namespace

TEST_F(ChargeControllerTest, PeakShiftNoPolicies) {
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.peak_shift_enabled());
}

TEST_F(ChargeControllerTest, PeakShiftThresholdOnly) {
  constexpr int kThreshold = 50;
  policy_.set_peak_shift_battery_percent_threshold(kThreshold);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.peak_shift_enabled());
}

TEST_F(ChargeControllerTest, PeakShiftDayConfigsOnly) {
  constexpr PowerManagementPolicy::WeekDay kDay = PowerManagementPolicy::MONDAY;
  constexpr char kDayConfig[] = "00 30 09 45 20 00";

  MakePeakShiftDayConfig(kDay, kDayConfig,
                         policy_.add_peak_shift_day_configs());
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.peak_shift_enabled());
}

TEST_F(ChargeControllerTest, PeakShiftThresholdAndDayConfigs) {
  constexpr int kThreshold = 50;

  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::MONDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::FRIDAY;

  constexpr char kDayConfig1[] = "00 30 09 45 20 00";
  constexpr char kDayConfig2[] = "09 15 10 00 23 15";

  policy_.set_peak_shift_battery_percent_threshold(kThreshold);
  MakePeakShiftDayConfig(kDay1, kDayConfig1,
                         policy_.add_peak_shift_day_configs());
  MakePeakShiftDayConfig(kDay2, kDayConfig2,
                         policy_.add_peak_shift_day_configs());

  controller_.HandlePolicyChange(policy_);

  EXPECT_TRUE(helper_.peak_shift_enabled());
  EXPECT_EQ(helper_.peak_shift_threshold(), kThreshold);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay1), kDayConfig1);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay2), kDayConfig2);
}

TEST_F(ChargeControllerTest, PeakShiftTwiceWithNoChanges) {
  constexpr int kThreshold = 50;

  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::MONDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::FRIDAY;

  constexpr char kDayConfig1[] = "00 30 09 45 20 00";
  constexpr char kDayConfig2[] = "09 15 10 00 23 15";

  policy_.set_peak_shift_battery_percent_threshold(kThreshold);
  MakePeakShiftDayConfig(kDay1, kDayConfig1,
                         policy_.add_peak_shift_day_configs());
  MakePeakShiftDayConfig(kDay2, kDayConfig2,
                         policy_.add_peak_shift_day_configs());

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.peak_shift_enabled());
  EXPECT_EQ(helper_.peak_shift_threshold(), kThreshold);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay1), kDayConfig1);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay2), kDayConfig2);

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.peak_shift_enabled());
  EXPECT_EQ(helper_.peak_shift_threshold(),
            system::ChargeControllerHelperStub::kThresholdUnset);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay1), "");
  EXPECT_EQ(helper_.peak_shift_day_config(kDay2), "");
}

TEST_F(ChargeControllerTest, PeakShiftTwiceWithChanges) {
  constexpr int kThreshold1 = 50;

  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::MONDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::FRIDAY;

  constexpr char kDayConfig1[] = "00 30 09 45 20 00";
  constexpr char kDayConfig2[] = "09 15 10 00 23 15";

  policy_.set_peak_shift_battery_percent_threshold(kThreshold1);
  MakePeakShiftDayConfig(kDay1, kDayConfig1,
                         policy_.add_peak_shift_day_configs());
  MakePeakShiftDayConfig(kDay2, kDayConfig2,
                         policy_.add_peak_shift_day_configs());

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.peak_shift_enabled());
  EXPECT_EQ(helper_.peak_shift_threshold(), kThreshold1);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay1), kDayConfig1);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay2), kDayConfig2);

  helper_.Reset();

  constexpr int kThreshold2 = 20;
  policy_.set_peak_shift_battery_percent_threshold(kThreshold2);

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.peak_shift_enabled());
  EXPECT_EQ(helper_.peak_shift_threshold(), kThreshold2);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay1), kDayConfig1);
  EXPECT_EQ(helper_.peak_shift_day_config(kDay2), kDayConfig2);
}

TEST_F(ChargeControllerTest, BootOnAcEnabled) {
  policy_.set_boot_on_ac(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.boot_on_ac_enabled());
}

TEST_F(ChargeControllerTest, BootOnAcDisabled) {
  policy_.set_boot_on_ac(false);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.boot_on_ac_enabled());
}

TEST_F(ChargeControllerTest, BootOnAcTwiceNoChanges) {
  policy_.set_boot_on_ac(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.boot_on_ac_enabled());

  helper_.Reset();
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.boot_on_ac_enabled());
}

TEST_F(ChargeControllerTest, BootOnAcThriceChanges) {
  policy_.set_boot_on_ac(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.boot_on_ac_enabled());

  helper_.Reset();

  policy_.set_boot_on_ac(false);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.boot_on_ac_enabled());

  policy_.set_boot_on_ac(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.boot_on_ac_enabled());
}

TEST_F(ChargeControllerTest, UsbPowerShareEnabled) {
  policy_.set_usb_power_share(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.usb_power_share_enabled());
}

TEST_F(ChargeControllerTest, UsbPowerShareDisabled) {
  policy_.set_usb_power_share(false);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.usb_power_share_enabled());
}

TEST_F(ChargeControllerTest, UsbPowerShareTwiceNoChanges) {
  policy_.set_usb_power_share(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.usb_power_share_enabled());

  helper_.Reset();
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.usb_power_share_enabled());
}

TEST_F(ChargeControllerTest, UsbPowerShareThriceChanges) {
  policy_.set_usb_power_share(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.usb_power_share_enabled());

  helper_.Reset();

  policy_.set_usb_power_share(false);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.usb_power_share_enabled());

  policy_.set_usb_power_share(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.usb_power_share_enabled());
}

TEST_F(ChargeControllerTest, AdvancedBatteryChargeModeEnabledNoPolicies) {
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.advanced_battery_charge_mode_enabled());
}

TEST_F(ChargeControllerTest, AdvancedBatteryChargeModeEnabledEnabled) {
  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::TUESDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::SUNDAY;

  constexpr char kDayConfigStartEnd1[] = "02 45 08 30";
  constexpr char kDayConfigStartEnd2[] = "03 30 16 00";

  MakeAdvancedBatteryChargeModeDayConfig(
      kDay1, kDayConfigStartEnd1,
      policy_.add_advanced_battery_charge_mode_day_configs());
  MakeAdvancedBatteryChargeModeDayConfig(
      kDay2, kDayConfigStartEnd2,
      policy_.add_advanced_battery_charge_mode_day_configs());

  controller_.HandlePolicyChange(policy_);

  constexpr char kDayConfigStartDuration1[] = "02 45 05 45";
  constexpr char kDayConfigStartDuration2[] = "03 30 12 30";

  EXPECT_TRUE(helper_.advanced_battery_charge_mode_enabled());
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay1),
            kDayConfigStartDuration1);
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay2),
            kDayConfigStartDuration2);
}

TEST_F(ChargeControllerTest,
       AdvancedBatteryChargeModeEnabledTwiceWithNoChanges) {
  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::WEDNESDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::MONDAY;

  constexpr char kDayConfigStartEnd1[] = "20 15 22 00";
  constexpr char kDayConfigStartEnd2[] = "10 00 23 15";

  MakeAdvancedBatteryChargeModeDayConfig(
      kDay1, kDayConfigStartEnd1,
      policy_.add_advanced_battery_charge_mode_day_configs());
  MakeAdvancedBatteryChargeModeDayConfig(
      kDay2, kDayConfigStartEnd2,
      policy_.add_advanced_battery_charge_mode_day_configs());

  constexpr char kDayConfigStartDuration1[] = "20 15 01 45";
  constexpr char kDayConfigStartDuration2[] = "10 00 13 15";

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.advanced_battery_charge_mode_enabled());
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay1),
            kDayConfigStartDuration1);
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay2),
            kDayConfigStartDuration2);

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.advanced_battery_charge_mode_enabled());
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay1), "");
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay2), "");
}

TEST_F(ChargeControllerTest, AdvancedBatteryChargeModeEnabledTwiceWithChanges) {
  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::FRIDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::SUNDAY;

  constexpr char kDayConfigStartEnd1[] = "08 15 10 45";
  constexpr char kDayConfigStartEnd2[] = "04 00 06 15";

  MakeAdvancedBatteryChargeModeDayConfig(
      kDay1, kDayConfigStartEnd1,
      policy_.add_advanced_battery_charge_mode_day_configs());
  MakeAdvancedBatteryChargeModeDayConfig(
      kDay2, kDayConfigStartEnd2,
      policy_.add_advanced_battery_charge_mode_day_configs());

  constexpr char kDayConfigStartDuration1[] = "08 15 02 30";
  constexpr char kDayConfigStartDuration2[] = "04 00 02 15";

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.advanced_battery_charge_mode_enabled());
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay1),
            kDayConfigStartDuration1);
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay2),
            kDayConfigStartDuration2);

  helper_.Reset();

  constexpr char kDayConfigStartEnd3[] = "09 00 11 15";
  constexpr char kDayConfigStartEnd4[] = "05 45 16 30";

  MakeAdvancedBatteryChargeModeDayConfig(
      kDay1, kDayConfigStartEnd3,
      policy_.mutable_advanced_battery_charge_mode_day_configs(0));
  MakeAdvancedBatteryChargeModeDayConfig(
      kDay2, kDayConfigStartEnd4,
      policy_.mutable_advanced_battery_charge_mode_day_configs(1));

  constexpr char kDayConfigStartDuration3[] = "09 00 02 15";
  constexpr char kDayConfigStartDuration4[] = "05 45 10 45";

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.advanced_battery_charge_mode_enabled());
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay1),
            kDayConfigStartDuration3);
  EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(kDay2),
            kDayConfigStartDuration4);
}

}  // namespace policy
}  // namespace power_manager
