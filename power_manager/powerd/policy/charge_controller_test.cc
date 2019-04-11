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
    system::ChargeControllerHelperInterface::WeekDay week_day,
    const std::string& config_str,
    PowerManagementPolicy::PeakShiftDayConfig* config_proto) {
  DCHECK(config_proto);

  switch (week_day) {
    case system::ChargeControllerHelperInterface::WeekDay::MONDAY:
      config_proto->set_day(PowerManagementPolicy::MONDAY);
      break;
    case system::ChargeControllerHelperInterface::WeekDay::TUESDAY:
      config_proto->set_day(PowerManagementPolicy::TUESDAY);
      break;
    case system::ChargeControllerHelperInterface::WeekDay::WEDNESDAY:
      config_proto->set_day(PowerManagementPolicy::WEDNESDAY);
      break;
    case system::ChargeControllerHelperInterface::WeekDay::THURSDAY:
      config_proto->set_day(PowerManagementPolicy::THURSDAY);
      break;
    case system::ChargeControllerHelperInterface::WeekDay::FRIDAY:
      config_proto->set_day(PowerManagementPolicy::FRIDAY);
      break;
    case system::ChargeControllerHelperInterface::WeekDay::SATURDAY:
      config_proto->set_day(PowerManagementPolicy::SATURDAY);
      break;
    case system::ChargeControllerHelperInterface::WeekDay::SUNDAY:
      config_proto->set_day(PowerManagementPolicy::SUNDAY);
      break;
  }

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
  constexpr system::ChargeControllerHelperInterface::WeekDay kDay =
      system::ChargeControllerHelperInterface::WeekDay::MONDAY;
  constexpr char kDayConfig[] = "00 30 09 45 20 00";

  MakePeakShiftDayConfig(kDay, kDayConfig,
                         policy_.add_peak_shift_day_configs());
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.peak_shift_enabled());
}

TEST_F(ChargeControllerTest, PeakShiftThresholdAndDayConfigs) {
  constexpr int kThreshold = 50;

  constexpr system::ChargeControllerHelperInterface::WeekDay kDay1 =
      system::ChargeControllerHelperInterface::WeekDay::MONDAY;
  constexpr system::ChargeControllerHelperInterface::WeekDay kDay2 =
      system::ChargeControllerHelperInterface::WeekDay::FRIDAY;

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

  constexpr system::ChargeControllerHelperInterface::WeekDay kDay1 =
      system::ChargeControllerHelperInterface::WeekDay::MONDAY;
  constexpr system::ChargeControllerHelperInterface::WeekDay kDay2 =
      system::ChargeControllerHelperInterface::WeekDay::FRIDAY;

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

  constexpr system::ChargeControllerHelperInterface::WeekDay kDay1 =
      system::ChargeControllerHelperInterface::WeekDay::MONDAY;
  constexpr system::ChargeControllerHelperInterface::WeekDay kDay2 =
      system::ChargeControllerHelperInterface::WeekDay::FRIDAY;

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

}  // namespace policy
}  // namespace power_manager
