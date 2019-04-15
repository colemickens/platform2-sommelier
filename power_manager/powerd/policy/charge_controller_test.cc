// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/charge_controller.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/strings/string_split.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
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

  std::vector<std::string> split = base::SplitString(
      config_str, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(split.size(), 6);

  int value;
  ASSERT_TRUE(base::StringToInt(split[0], &value));
  config_proto->mutable_start_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(split[1], &value));
  config_proto->mutable_start_time()->set_minute(value);
  ASSERT_TRUE(base::StringToInt(split[2], &value));
  config_proto->mutable_end_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(split[3], &value));
  config_proto->mutable_end_time()->set_minute(value);
  ASSERT_TRUE(base::StringToInt(split[4], &value));
  config_proto->mutable_charge_start_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(split[5], &value));
  config_proto->mutable_charge_start_time()->set_minute(value);
}

void MakeAdvancedBatteryChargeModeDayConfig(
    PowerManagementPolicy::WeekDay week_day,
    const std::string& config_str,
    PowerManagementPolicy::AdvancedBatteryChargeModeDayConfig* config_proto) {
  DCHECK(config_proto);

  config_proto->set_day(week_day);

  std::vector<std::string> split = base::SplitString(
      config_str, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(split.size(), 4);

  int value;
  ASSERT_TRUE(base::StringToInt(split[0], &value));
  config_proto->mutable_charge_start_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(split[1], &value));
  config_proto->mutable_charge_start_time()->set_minute(value);
  ASSERT_TRUE(base::StringToInt(split[2], &value));
  config_proto->mutable_charge_end_time()->set_hour(value);
  ASSERT_TRUE(base::StringToInt(split[3], &value));
  config_proto->mutable_charge_end_time()->set_minute(value);
}

// Converts string with encoded start time and end time to equivalent string
// with encoded start time and duration.
// "02 00 10 15" which encodes time interval from 02:00 to 10:15 will be
// converted to "02 00 08 15" because interval duration is 08:15.
void ConvertStartEndToStartDuration(const std::string& start_end,
                                    std::string* start_duration_out) {
  DCHECK(start_duration_out);

  std::vector<std::string> split = base::SplitString(
      start_end, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(split.size(), 4);

  int start_hour, start_minute, end_hour, end_minute;
  ASSERT_TRUE(base::StringToInt(split[0], &start_hour));
  ASSERT_TRUE(base::StringToInt(split[1], &start_minute));
  ASSERT_TRUE(base::StringToInt(split[2], &end_hour));
  ASSERT_TRUE(base::StringToInt(split[3], &end_minute));

  int duration_minute =
      (end_hour * 60 + end_minute) - (start_hour * 60 + start_minute);
  *start_duration_out =
      base::StringPrintf("%02d %02d %02d %02d", start_hour, start_minute,
                         duration_minute / 60, duration_minute % 60);
}

class ChargeControllerTest : public ::testing::Test {
 public:
  ChargeControllerTest() { controller_.Init(&helper_); }

 protected:
  // Sets PeakShift policy in PowerManagementPolicy proto.
  void SetPeakShift(
      int threshold,
      const std::vector<std::pair<PowerManagementPolicy::WeekDay, std::string>>&
          day_configs) {
    policy_.set_peak_shift_battery_percent_threshold(threshold);
    policy_.mutable_peak_shift_day_configs()->Clear();
    for (const auto& item : day_configs) {
      MakePeakShiftDayConfig(item.first, item.second,
                             policy_.add_peak_shift_day_configs());
    }
  }

  // Checks that PeakShift policy was applied as expected.
  bool CheckPeakShift(
      bool enable,
      int threshold,
      const std::vector<std::pair<PowerManagementPolicy::WeekDay, std::string>>&
          day_configs) {
    for (const auto& item : day_configs) {
      EXPECT_EQ(helper_.peak_shift_day_config(item.first), item.second);
      if (helper_.peak_shift_day_config(item.first) != item.second) {
        return false;
      }
    }
    EXPECT_EQ(helper_.peak_shift_enabled(), enable);
    EXPECT_EQ(helper_.peak_shift_threshold(), threshold);
    return helper_.peak_shift_enabled() == enable &&
           helper_.peak_shift_threshold() == threshold;
  }

  // Sets AdvancedBatteryChargeMode policy in PowerManagementPolicy proto.
  void SetAdvancedBatteryChargeMode(
      const std::vector<std::pair<PowerManagementPolicy::WeekDay, std::string>>&
          day_configs) {
    policy_.mutable_advanced_battery_charge_mode_day_configs()->Clear();
    for (const auto& item : day_configs) {
      MakeAdvancedBatteryChargeModeDayConfig(
          item.first, item.second,
          policy_.add_advanced_battery_charge_mode_day_configs());
    }
  }

  // Checks that AdvancedBatteryChargeMode policy was applied as expected.
  bool CheckAdvancedBatteryChargeMode(
      bool enable,
      const std::vector<std::pair<PowerManagementPolicy::WeekDay, std::string>>&
          day_configs) {
    for (const auto& item : day_configs) {
      std::string start_duration;
      if (item.second.length()) {
        ConvertStartEndToStartDuration(item.second, &start_duration);
      }
      EXPECT_EQ(helper_.advanced_battery_charge_mode_day_config(item.first),
                start_duration);
      if (helper_.advanced_battery_charge_mode_day_config(item.first) !=
          start_duration) {
        return false;
      }
    }
    EXPECT_EQ(helper_.advanced_battery_charge_mode_enabled(), enable);
    return helper_.advanced_battery_charge_mode_enabled() == enable;
  }

  // Sets BatteryChargeMode policy in PowerManagementPolicy proto.
  void SetBatteryChargeMode(PowerManagementPolicy::BatteryChargeMode::Mode mode,
                            int custom_charge_start,
                            int custom_charge_stop) {
    policy_.mutable_battery_charge_mode()->set_mode(mode);
    policy_.mutable_battery_charge_mode()->set_custom_charge_start(
        custom_charge_start);
    policy_.mutable_battery_charge_mode()->set_custom_charge_stop(
        custom_charge_stop);
  }

  // Checks that BatteryChargeMode policy was applied as expected.
  bool CheckBatteryChargeMode(
      PowerManagementPolicy::BatteryChargeMode::Mode mode,
      int custom_charge_start =
          system::ChargeControllerHelperStub::kCustomChargeThresholdUnset,
      int custom_charge_stop =
          system::ChargeControllerHelperStub::kCustomChargeThresholdUnset) {
    EXPECT_EQ(helper_.battery_charge_mode(), mode);
    EXPECT_EQ(helper_.custom_charge_start(), custom_charge_start);
    EXPECT_EQ(helper_.custom_charge_stop(), custom_charge_stop);
    return helper_.battery_charge_mode() == mode &&
           helper_.custom_charge_start() == custom_charge_start &&
           helper_.custom_charge_stop() == custom_charge_stop;
  }

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

TEST_F(ChargeControllerTest, PeakShift) {
  constexpr int kThreshold1 = 50;
  constexpr int kThreshold2 = 45;

  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::MONDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::FRIDAY;

  constexpr char kDayConfig1[] = "00 30 09 45 20 00";
  constexpr char kDayConfig2[] = "09 15 10 00 23 15";

  SetPeakShift(kThreshold1, {{kDay1, kDayConfig1}, {kDay2, kDayConfig2}});
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckPeakShift(true, kThreshold1,
                             {{kDay1, kDayConfig1}, {kDay2, kDayConfig2}}));

  SetPeakShift(kThreshold2, {{kDay1, kDayConfig2}, {kDay2, kDayConfig1}});
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckPeakShift(true, kThreshold2,
                             {{kDay1, kDayConfig2}, {kDay2, kDayConfig1}}));

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckPeakShift(
      false, system::ChargeControllerHelperStub::kPeakShiftThresholdUnset,
      {{kDay1, ""}, {kDay2, ""}}));
}

TEST_F(ChargeControllerTest, BootOnAc) {
  policy_.set_boot_on_ac(false);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.boot_on_ac_enabled());

  policy_.set_boot_on_ac(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.boot_on_ac_enabled());

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.boot_on_ac_enabled());
}

TEST_F(ChargeControllerTest, UsbPowerShare) {
  policy_.set_usb_power_share(false);
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.usb_power_share_enabled());

  policy_.set_usb_power_share(true);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(helper_.usb_power_share_enabled());

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.usb_power_share_enabled());
}

TEST_F(ChargeControllerTest, AdvancedBatteryChargeModeEnabledNoPolicies) {
  controller_.HandlePolicyChange(policy_);
  EXPECT_FALSE(helper_.advanced_battery_charge_mode_enabled());
}

TEST_F(ChargeControllerTest, AdvancedBatteryChargeMode) {
  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::TUESDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::SUNDAY;

  constexpr char kDayConfigStartEnd1[] = "02 45 08 30";
  constexpr char kDayConfigStartEnd2[] = "03 30 16 00";

  SetAdvancedBatteryChargeMode(
      {{kDay1, kDayConfigStartEnd1}, {kDay2, kDayConfigStartEnd2}});
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckAdvancedBatteryChargeMode(
      true, {{kDay1, kDayConfigStartEnd1}, {kDay2, kDayConfigStartEnd2}}));

  SetAdvancedBatteryChargeMode(
      {{kDay1, kDayConfigStartEnd2}, {kDay2, kDayConfigStartEnd1}});
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckAdvancedBatteryChargeMode(
      true, {{kDay1, kDayConfigStartEnd2}, {kDay2, kDayConfigStartEnd1}}));

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(
      CheckAdvancedBatteryChargeMode(false, {{kDay1, ""}, {kDay2, ""}}));
}

TEST_F(ChargeControllerTest, BatteryChargeModeNoPolicies) {
  EXPECT_TRUE(CheckBatteryChargeMode(
      system::ChargeControllerHelperStub::kBatteryChargeModeUnset));

  controller_.HandlePolicyChange(policy_);

  EXPECT_TRUE(CheckBatteryChargeMode(
      PowerManagementPolicy::BatteryChargeMode::STANDARD));
}

TEST_F(ChargeControllerTest, BatteryChargeMode) {
  constexpr PowerManagementPolicy::BatteryChargeMode::Mode kMode1 =
      PowerManagementPolicy::BatteryChargeMode::PRIMARILY_AC_USE;
  constexpr PowerManagementPolicy::BatteryChargeMode::Mode kMode2 =
      PowerManagementPolicy::BatteryChargeMode::CUSTOM;
  constexpr int kCustomStartCharge = 60;
  constexpr int kCustomEndCharge = 90;

  policy_.mutable_battery_charge_mode()->set_mode(kMode1);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckBatteryChargeMode(kMode1));

  SetBatteryChargeMode(kMode2, kCustomStartCharge, kCustomEndCharge);
  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(
      CheckBatteryChargeMode(kMode2, kCustomStartCharge, kCustomEndCharge));

  helper_.Reset();

  controller_.HandlePolicyChange(policy_);
  EXPECT_TRUE(CheckBatteryChargeMode(
      system::ChargeControllerHelperStub::kBatteryChargeModeUnset));
}

// If AdvancedBatteryChargeMode is specified, it overrides BatteryChargeMode.
TEST_F(ChargeControllerTest,
       AdvancedBatteryChargeModeOverridesBatteryChargeMode) {
  constexpr PowerManagementPolicy::BatteryChargeMode::Mode kMode =
      PowerManagementPolicy::BatteryChargeMode::EXPRESS_CHARGE;

  constexpr PowerManagementPolicy::WeekDay kDay1 =
      PowerManagementPolicy::FRIDAY;
  constexpr PowerManagementPolicy::WeekDay kDay2 =
      PowerManagementPolicy::SUNDAY;

  constexpr char kDayConfigStartEnd1[] = "08 15 10 45";
  constexpr char kDayConfigStartEnd2[] = "04 00 06 15";

  policy_.mutable_battery_charge_mode()->set_mode(kMode);
  SetAdvancedBatteryChargeMode(
      {{kDay1, kDayConfigStartEnd1}, {kDay2, kDayConfigStartEnd2}});

  controller_.HandlePolicyChange(policy_);

  EXPECT_EQ(helper_.battery_charge_mode(),
            system::ChargeControllerHelperStub::kBatteryChargeModeUnset);
  EXPECT_TRUE(CheckAdvancedBatteryChargeMode(
      true, {{kDay1, kDayConfigStartEnd1}, {kDay2, kDayConfigStartEnd2}}));
}

}  // namespace policy
}  // namespace power_manager
