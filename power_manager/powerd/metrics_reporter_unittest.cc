// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/metrics_reporter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <string>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/service_constants.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/policy/backlight_controller_stub.h"
#include "power_manager/powerd/system/power_supply.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;

namespace power_manager {

class MetricsReporterTest : public Test {
 public:
  MetricsReporterTest() {
    metrics_reporter_.clock_.set_current_time_for_testing(
        base::TimeTicks::FromInternalValue(1000));
    metrics_reporter_.clock_.set_current_wall_time_for_testing(
        base::Time::FromInternalValue(2000));

    power_status_.battery_percentage = 100.0;
    power_status_.battery_is_present = true;
  }

 protected:
  // Initializes |metrics_reporter_|.
  void Init() {
    metrics_reporter_.Init(&prefs_, &metrics_lib_,
                           &display_backlight_controller_,
                           &keyboard_backlight_controller_, power_status_);
  }

  // Advances both the monotonically-increasing time and wall time by
  // |interval|.
  void AdvanceTime(base::TimeDelta interval) {
    metrics_reporter_.clock_.set_current_time_for_testing(
        metrics_reporter_.clock_.GetCurrentTime() + interval);
    metrics_reporter_.clock_.set_current_wall_time_for_testing(
        metrics_reporter_.clock_.GetCurrentWallTime() + interval);
  }

  // Adds expectations to ignore all metrics sent by HandleSessionStateChange()
  // (except ones listed in |metrics_to_test_|).
  void IgnoreHandleSessionStateChangeMetrics() {
    IgnoreEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
        kMetricBatteryRemainingAtStartOfSessionName, POWER_AC));
    IgnoreEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
        kMetricBatteryRemainingAtStartOfSessionName, POWER_BATTERY));
    IgnoreEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
        kMetricBatteryRemainingAtEndOfSessionName, POWER_AC));
    IgnoreEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
        kMetricBatteryRemainingAtEndOfSessionName, POWER_BATTERY));
    IgnoreMetric(kMetricLengthOfSessionName);
    IgnoreMetric(kMetricNumberOfAlsAdjustmentsPerSessionName);
    IgnoreMetric(MetricsReporter::AppendPowerSourceToEnumName(
        kMetricUserBrightnessAdjustmentsPerSessionName, POWER_AC));
    IgnoreMetric(MetricsReporter::AppendPowerSourceToEnumName(
        kMetricUserBrightnessAdjustmentsPerSessionName, POWER_BATTERY));
  }

  // Adds expectations to ignore all metrics sent by HandlePowerStatusUpdate()
  // (except ones listed in |metrics_to_test_|).
  void IgnoreHandlePowerStatusUpdateMetrics() {
    IgnoreMetric(kMetricNumOfSessionsPerChargeName);
    IgnoreEnumMetric(kMetricBatteryRemainingWhenChargeStartsName);
    IgnoreEnumMetric(kMetricBatteryChargeHealthName);
    IgnoreMetric(kMetricBatteryDischargeRateName);
    IgnoreMetric(kMetricBatteryDischargeRateWhileSuspendedName);
    IgnoreEnumMetric(kMetricBatteryInfoSampleName);
  }

  // Updates |power_status_|'s |line_power_on| member and passes it to
  // HandlePowerStatusUpdate().
  void UpdatePowerStatusLinePower(bool line_power_on) {
    power_status_.line_power_on = line_power_on;
    metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  }

  // Adds a metrics library mock expectation that the specified metric
  // will be generated.
  void ExpectMetric(const std::string& name,
                    int sample,
                    int min,
                    int max,
                    int buckets) {
    EXPECT_CALL(metrics_lib_, SendToUMA(name, sample, min, max, buckets))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  // Adds a metrics library mock expectation that the specified enum
  // metric will be generated.
  void ExpectEnumMetric(const std::string& name, int sample, int max) {
    EXPECT_CALL(metrics_lib_, SendEnumToUMA(name, sample, max))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  // Ignores an arbitrary number of reports of |name|.
  void IgnoreMetric(const std::string& name) {
    if (metrics_to_test_.count(name))
      return;
    EXPECT_CALL(metrics_lib_, SendToUMA(name, _, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
  }

  // Ignores an arbitrary number of reports of |name|.
  void IgnoreEnumMetric(const std::string& name) {
    if (metrics_to_test_.count(name))
      return;
    EXPECT_CALL(metrics_lib_, SendEnumToUMA(name, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
  }

  void ExpectBatteryDischargeRateMetric(int sample) {
    ExpectMetric(kMetricBatteryDischargeRateName, sample,
                 kMetricBatteryDischargeRateMin,
                 kMetricBatteryDischargeRateMax,
                 kMetricDefaultBuckets);
  }

  void ExpectNumOfSessionsPerChargeMetric(int sample) {
    ExpectMetric(kMetricNumOfSessionsPerChargeName, sample,
                 kMetricNumOfSessionsPerChargeMin,
                 kMetricNumOfSessionsPerChargeMax,
                 kMetricDefaultBuckets);
  }

  FakePrefs prefs_;
  policy::BacklightControllerStub display_backlight_controller_;
  policy::BacklightControllerStub keyboard_backlight_controller_;
  system::PowerStatus power_status_;

  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;

  MetricsReporter metrics_reporter_;

  // Names of metrics that will not be ignored by calls to Ignore*(). Tests
  // should insert the metrics that they're testing into this set and then call
  // Ignore*Metrics() (and call it again whenever expectations are cleared).
  std::set<std::string> metrics_to_test_;
};

TEST_F(MetricsReporterTest, BacklightLevel) {
  power_status_.line_power_on = false;
  Init();
  ASSERT_TRUE(metrics_reporter_.generate_backlight_metrics_timer_.IsRunning());
  metrics_reporter_.HandleScreenDimmedChange(true, base::TimeTicks::Now());
  metrics_reporter_.GenerateBacklightLevelMetrics();
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  const int64 kCurrentDisplayPercent = 57;
  display_backlight_controller_.set_percent(kCurrentDisplayPercent);
  const int64 kCurrentKeyboardPercent = 43;
  keyboard_backlight_controller_.set_percent(kCurrentKeyboardPercent);

  metrics_reporter_.HandleScreenDimmedChange(false, base::TimeTicks::Now());
  ExpectEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
                       kMetricBacklightLevelName, POWER_BATTERY),
                   kCurrentDisplayPercent, kMetricMaxPercent);
  ExpectEnumMetric(kMetricKeyboardBacklightLevelName, kCurrentKeyboardPercent,
                   kMetricMaxPercent);
  metrics_reporter_.GenerateBacklightLevelMetrics();

  power_status_.line_power_on = true;
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  ExpectEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
                       kMetricBacklightLevelName, POWER_AC),
                   kCurrentDisplayPercent, kMetricMaxPercent);
  ExpectEnumMetric(kMetricKeyboardBacklightLevelName, kCurrentKeyboardPercent,
                   kMetricMaxPercent);
  metrics_reporter_.GenerateBacklightLevelMetrics();
}

TEST_F(MetricsReporterTest, BatteryDischargeRate) {
  power_status_.line_power_on = false;
  Init();

  metrics_to_test_.insert(kMetricBatteryDischargeRateName);
  IgnoreHandlePowerStatusUpdateMetrics();

  // This much time must elapse before the discharge rate will be reported
  // again.
  const base::TimeDelta interval = base::TimeDelta::FromSeconds(
      kMetricBatteryDischargeRateInterval);

  power_status_.battery_energy_rate = 5.0;
  ExpectBatteryDischargeRateMetric(5000);
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);

  power_status_.battery_energy_rate = 4.5;
  ExpectBatteryDischargeRateMetric(4500);
  AdvanceTime(interval);
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);

  power_status_.battery_energy_rate = 6.4;
  ExpectBatteryDischargeRateMetric(6400);
  AdvanceTime(interval);
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);

  Mock::VerifyAndClearExpectations(&metrics_lib_);
  IgnoreHandlePowerStatusUpdateMetrics();

  // Another update before the full interval has elapsed shouldn't result in
  // another report.
  AdvanceTime(interval / 2);
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);

  // Neither should a call while the energy rate is negative.
  AdvanceTime(interval);
  power_status_.battery_energy_rate = -4.0;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);

  // Ditto for a call while the system is on AC power.
  power_status_.line_power_on = true;
  power_status_.battery_energy_rate = 4.0;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
}

TEST_F(MetricsReporterTest, BatteryInfoWhenChargeStarts) {
  const double kBatteryPercentages[] = { 10.1, 10.7, 82.4, 82.5, 100.0 };

  power_status_.line_power_on = false;
  power_status_.battery_charge_full_design = 100.0;
  Init();

  metrics_to_test_.insert(kMetricBatteryRemainingWhenChargeStartsName);
  metrics_to_test_.insert(kMetricBatteryChargeHealthName);

  for (size_t i = 0; i < arraysize(kBatteryPercentages); ++i) {
    IgnoreHandlePowerStatusUpdateMetrics();

    power_status_.line_power_on = false;
    power_status_.battery_charge_full = kBatteryPercentages[i];
    power_status_.battery_percentage = kBatteryPercentages[i];
    metrics_reporter_.HandlePowerStatusUpdate(power_status_);

    power_status_.line_power_on = true;
    ExpectEnumMetric(kMetricBatteryRemainingWhenChargeStartsName,
                     round(power_status_.battery_percentage),
                     kMetricMaxPercent);
    ExpectEnumMetric(kMetricBatteryChargeHealthName,
                     round(100.0 * power_status_.battery_charge_full /
                           power_status_.battery_charge_full_design),
                     kMetricBatteryChargeHealthMax);
    metrics_reporter_.HandlePowerStatusUpdate(power_status_);

    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(MetricsReporterTest, SessionStartOrStop) {
  const uint kAlsAdjustments[] = { 0, 100 };
  const uint kUserAdjustments[] = { 0, 200 };
  const double kBatteryPercentages[] = { 10.5, 23.0 };
  const int kSessionSecs[] = { 900, kMetricLengthOfSessionMax + 10 };
  ASSERT_EQ(arraysize(kAlsAdjustments), arraysize(kUserAdjustments));
  ASSERT_EQ(arraysize(kAlsAdjustments), arraysize(kBatteryPercentages));
  ASSERT_EQ(arraysize(kAlsAdjustments), arraysize(kSessionSecs));

  power_status_.line_power_on = false;
  Init();

  for (size_t i = 0; i < arraysize(kAlsAdjustments); ++i) {
    IgnoreHandlePowerStatusUpdateMetrics();
    power_status_.battery_percentage = kBatteryPercentages[i];
    ExpectEnumMetric(
        MetricsReporter::AppendPowerSourceToEnumName(
            kMetricBatteryRemainingAtStartOfSessionName, POWER_BATTERY),
        round(kBatteryPercentages[i]), kMetricMaxPercent);
    metrics_reporter_.HandlePowerStatusUpdate(power_status_);
    metrics_reporter_.HandleSessionStateChange(SESSION_STARTED);
    Mock::VerifyAndClearExpectations(&metrics_lib_);

    ExpectEnumMetric(
        MetricsReporter::AppendPowerSourceToEnumName(
            kMetricBatteryRemainingAtEndOfSessionName, POWER_BATTERY),
        round(kBatteryPercentages[i]), kMetricMaxPercent);

    display_backlight_controller_.set_num_als_adjustments(kAlsAdjustments[i]);
    display_backlight_controller_.set_num_user_adjustments(kUserAdjustments[i]);
    ExpectMetric(kMetricNumberOfAlsAdjustmentsPerSessionName,
                 kAlsAdjustments[i],
                 kMetricNumberOfAlsAdjustmentsPerSessionMin,
                 kMetricNumberOfAlsAdjustmentsPerSessionMax,
                 kMetricDefaultBuckets);
    ExpectMetric(
        MetricsReporter::AppendPowerSourceToEnumName(
            kMetricUserBrightnessAdjustmentsPerSessionName, POWER_BATTERY),
        kUserAdjustments[i],
        kMetricUserBrightnessAdjustmentsPerSessionMin,
        kMetricUserBrightnessAdjustmentsPerSessionMax,
        kMetricDefaultBuckets);

    AdvanceTime(base::TimeDelta::FromSeconds(kSessionSecs[i]));
    ExpectMetric(kMetricLengthOfSessionName,
                 std::min(kSessionSecs[i], kMetricLengthOfSessionMax),
                 kMetricLengthOfSessionMin,
                 kMetricLengthOfSessionMax,
                 kMetricDefaultBuckets);

    metrics_reporter_.HandleSessionStateChange(SESSION_STOPPED);
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(MetricsReporterTest, GenerateNumOfSessionsPerChargeMetric) {
  metrics_to_test_.insert(kMetricNumOfSessionsPerChargeName);
  power_status_.line_power_on = false;
  Init();

  IgnoreHandlePowerStatusUpdateMetrics();
  UpdatePowerStatusLinePower(true);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // If the session is already started when going off line power, it should be
  // counted. Additional power status updates that don't describe a power source
  // change shouldn't increment the count.
  IgnoreHandleSessionStateChangeMetrics();
  metrics_reporter_.HandleSessionStateChange(SESSION_STARTED);
  IgnoreHandlePowerStatusUpdateMetrics();
  UpdatePowerStatusLinePower(false);
  UpdatePowerStatusLinePower(false);
  UpdatePowerStatusLinePower(false);
  ExpectNumOfSessionsPerChargeMetric(1);
  UpdatePowerStatusLinePower(true);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Sessions that start while on battery power should also be counted.
  IgnoreHandleSessionStateChangeMetrics();
  metrics_reporter_.HandleSessionStateChange(SESSION_STOPPED);
  IgnoreHandlePowerStatusUpdateMetrics();
  UpdatePowerStatusLinePower(false);
  metrics_reporter_.HandleSessionStateChange(SESSION_STARTED);
  metrics_reporter_.HandleSessionStateChange(SESSION_STOPPED);
  metrics_reporter_.HandleSessionStateChange(SESSION_STARTED);
  metrics_reporter_.HandleSessionStateChange(SESSION_STOPPED);
  metrics_reporter_.HandleSessionStateChange(SESSION_STARTED);
  ExpectNumOfSessionsPerChargeMetric(3);
  UpdatePowerStatusLinePower(true);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Check that the pref is used, so the count will persist across reboots.
  IgnoreHandlePowerStatusUpdateMetrics();
  UpdatePowerStatusLinePower(false);
  prefs_.SetInt64(kNumSessionsOnCurrentChargePref, 5);
  ExpectNumOfSessionsPerChargeMetric(5);
  UpdatePowerStatusLinePower(true);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Negative values in the pref should be ignored.
  prefs_.SetInt64(kNumSessionsOnCurrentChargePref, -2);
  IgnoreHandlePowerStatusUpdateMetrics();
  UpdatePowerStatusLinePower(false);
  ExpectNumOfSessionsPerChargeMetric(1);
  UpdatePowerStatusLinePower(true);
  Mock::VerifyAndClearExpectations(&metrics_lib_);
}

TEST_F(MetricsReporterTest, SendEnumMetric) {
  Init();
  ExpectEnumMetric("Dummy.EnumMetric", 50, 200);
  EXPECT_TRUE(metrics_reporter_.SendEnumMetric("Dummy.EnumMetric", 50, 200));

  // Out-of-bounds values should be capped.
  ExpectEnumMetric("Dummy.EnumMetric2", 20, 20);
  EXPECT_TRUE(metrics_reporter_.SendEnumMetric("Dummy.EnumMetric2", 21, 20));
}

TEST_F(MetricsReporterTest, SendMetric) {
  Init();
  ExpectMetric("Dummy.Metric", 3, 1, 100, 50);
  EXPECT_TRUE(metrics_reporter_.SendMetric("Dummy.Metric", 3, 1, 100, 50));

  // Out-of-bounds values should be capped.
  ExpectMetric("Dummy.Metric2", 0, 0, 20, 4);
  EXPECT_TRUE(metrics_reporter_.SendMetric("Dummy.Metric2", -1, 0, 20, 4));
  ExpectMetric("Dummy.Metric3", 25, 5, 25, 6);
  EXPECT_TRUE(metrics_reporter_.SendMetric("Dummy.Metric3", 30, 5, 25, 6));
}

TEST_F(MetricsReporterTest, SendMetricWithPowerSource) {
  power_status_.line_power_on = false;
  Init();
  ExpectMetric("Dummy.MetricOnBattery", 3, 1, 100, 50);
  EXPECT_TRUE(metrics_reporter_.SendMetricWithPowerSource(
      "Dummy.Metric", 3, 1, 100, 50));

  IgnoreHandlePowerStatusUpdateMetrics();
  power_status_.line_power_on = true;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  ExpectMetric("Dummy.MetricOnAC", 6, 2, 200, 80);
  EXPECT_TRUE(metrics_reporter_.SendMetricWithPowerSource(
      "Dummy.Metric", 6, 2, 200, 80));
}

TEST_F(MetricsReporterTest, PowerButtonDownMetric) {
  Init();

  // We should ignore a button release that wasn't preceded by a press.
  metrics_reporter_.HandlePowerButtonEvent(BUTTON_UP);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Presses that are followed by additional presses should also be ignored.
  metrics_reporter_.HandlePowerButtonEvent(BUTTON_DOWN);
  metrics_reporter_.HandlePowerButtonEvent(BUTTON_DOWN);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Send a regular sequence of events and check that the duration is reported.
  metrics_reporter_.HandlePowerButtonEvent(BUTTON_DOWN);
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(243);
  AdvanceTime(kDuration);
  ExpectMetric(kMetricPowerButtonDownTimeName,
               kDuration.InMilliseconds(),
               kMetricPowerButtonDownTimeMin,
               kMetricPowerButtonDownTimeMax,
               kMetricDefaultBuckets);
  metrics_reporter_.HandlePowerButtonEvent(BUTTON_UP);
}

TEST_F(MetricsReporterTest, BatteryDischargeRateWhileSuspended) {
  const double kEnergyBeforeSuspend = 60;
  const double kEnergyAfterResume = 50;
  const base::TimeDelta kSuspendDuration = base::TimeDelta::FromHours(1);

  metrics_to_test_.insert(kMetricBatteryDischargeRateWhileSuspendedName);
  power_status_.line_power_on = false;
  power_status_.battery_energy = kEnergyAfterResume;
  Init();

  // We shouldn't send a sample if we haven't suspended.
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Ditto if the system is on AC before suspending...
  power_status_.line_power_on = true;
  power_status_.battery_energy = kEnergyBeforeSuspend;
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  metrics_reporter_.PrepareForSuspend();
  AdvanceTime(kSuspendDuration);
  ExpectMetric(kMetricSuspendAttemptsBeforeSuccessName, 1,
               kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
               kMetricSuspendAttemptsBuckets);
  metrics_reporter_.HandleResume(1);
  power_status_.line_power_on = false;
  power_status_.battery_energy = kEnergyAfterResume;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // ... or after resuming...
  power_status_.line_power_on = false;
  power_status_.battery_energy = kEnergyBeforeSuspend;
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  metrics_reporter_.PrepareForSuspend();
  AdvanceTime(kSuspendDuration);
  ExpectMetric(kMetricSuspendAttemptsBeforeSuccessName, 2,
               kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
               kMetricSuspendAttemptsBuckets);
  metrics_reporter_.HandleResume(2);
  power_status_.line_power_on = true;
  power_status_.battery_energy = kEnergyAfterResume;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // ... or if the battery's energy increased while the system was
  // suspended (i.e. it was temporarily connected to AC while suspended).
  power_status_.line_power_on = false;
  power_status_.battery_energy = kEnergyBeforeSuspend;
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  metrics_reporter_.PrepareForSuspend();
  AdvanceTime(kSuspendDuration);
  ExpectMetric(kMetricSuspendAttemptsBeforeSuccessName, 1,
               kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
               kMetricSuspendAttemptsBuckets);
  metrics_reporter_.HandleResume(1);
  power_status_.battery_energy = kEnergyBeforeSuspend + 5.0;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // The sample also shouldn't be reported if the system wasn't suspended
  // for very long.
  power_status_.battery_energy = kEnergyBeforeSuspend;
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  metrics_reporter_.PrepareForSuspend();
  AdvanceTime(base::TimeDelta::FromSeconds(
      kMetricBatteryDischargeRateWhileSuspendedMinSuspendSec - 1));
  ExpectMetric(kMetricSuspendAttemptsBeforeSuccessName, 1,
               kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
               kMetricSuspendAttemptsBuckets);
  metrics_reporter_.HandleResume(1);
  power_status_.battery_energy = kEnergyAfterResume;
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // The sample should be reported if the energy decreased over a long
  // enough time.
  power_status_.battery_energy = kEnergyBeforeSuspend;
  IgnoreHandlePowerStatusUpdateMetrics();
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
  metrics_reporter_.PrepareForSuspend();
  AdvanceTime(kSuspendDuration);
  ExpectMetric(kMetricSuspendAttemptsBeforeSuccessName, 1,
               kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
               kMetricSuspendAttemptsBuckets);
  metrics_reporter_.HandleResume(1);
  power_status_.battery_energy = kEnergyAfterResume;
  const int rate_mw = static_cast<int>(round(
      1000 * (kEnergyBeforeSuspend - kEnergyAfterResume) /
      (kSuspendDuration.InSecondsF() / 3600)));
  ExpectMetric(kMetricBatteryDischargeRateWhileSuspendedName, rate_mw,
               kMetricBatteryDischargeRateWhileSuspendedMin,
               kMetricBatteryDischargeRateWhileSuspendedMax,
               kMetricDefaultBuckets);
  metrics_reporter_.HandlePowerStatusUpdate(power_status_);
}

}  // namespace power_manager
