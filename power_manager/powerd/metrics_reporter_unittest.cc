// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/metrics_reporter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/system/power_supply.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::Test;

namespace power_manager {

namespace {

static const int64 kPowerButtonInterval = 20;
static const int kSessionLength = 5;
static const int kAdjustmentsOffset = 100;

// policy::BacklightController implementation that returns dummy values.
class BacklightControllerStub : public policy::BacklightController {
 public:
  BacklightControllerStub()
      : percent_(100.0),
        num_als_adjustments_(0),
        num_user_adjustments_(0) {
  }
  virtual ~BacklightControllerStub() {}

  void set_percent(double percent) { percent_ = percent; }
  void set_num_als_adjustments(int num) { num_als_adjustments_ = num; }
  void set_num_user_adjustments(int num) { num_user_adjustments_ = num; }

  // policy::BacklightController implementation:
  virtual void AddObserver(
      policy::BacklightControllerObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(
      policy::BacklightControllerObserver* observer) OVERRIDE {}
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE {}
  virtual void HandleDisplayModeChange(DisplayMode mode) OVERRIDE {}
  virtual void HandleSessionStateChange(SessionState state) OVERRIDE {}
  virtual void HandlePowerButtonPress() OVERRIDE {}
  virtual void HandleUserActivity() OVERRIDE {}
  virtual void SetDimmedForInactivity(bool dimmed) OVERRIDE {}
  virtual void SetOffForInactivity(bool off) OVERRIDE {}
  virtual void SetSuspended(bool suspended) OVERRIDE {}
  virtual void SetShuttingDown(bool shutting_down) OVERRIDE {}
  virtual void SetDocked(bool docked) OVERRIDE {}
  virtual bool GetBrightnessPercent(double* percent) OVERRIDE {
    *percent = percent_;
    return true;
  }
  virtual bool SetUserBrightnessPercent(double percent, TransitionStyle style)
      OVERRIDE {
    return true;
  }
  virtual bool IncreaseUserBrightness() OVERRIDE { return true; }
  virtual bool DecreaseUserBrightness(bool allow_off) OVERRIDE { return true; }
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE {
    return num_als_adjustments_;
  }
  virtual int GetNumUserAdjustments() const OVERRIDE {
    return num_user_adjustments_;
  }

 private:
  // Percent to be returned by GetBrightnessPercent().
  double percent_;

  // Counts to be returned by GetNum*Adjustments().
  int num_als_adjustments_;
  int num_user_adjustments_;

  DISALLOW_COPY_AND_ASSIGN(BacklightControllerStub);
};

}  // namespace

class MetricsReporterTest : public Test {
 public:
  MetricsReporterTest()
      : metrics_reporter_(&prefs_, &metrics_lib_,
                          &display_backlight_controller_,
                          &keyboard_backlight_controller_) {}

 protected:
  // Adds a metrics library mock expectation that the specified metric
  // will be generated.
  void ExpectMetric(const std::string& name, int sample,
                    int min, int max, int buckets) {
    EXPECT_CALL(metrics_lib_, SendToUMA(name, sample, min, max, buckets))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  void ExpectMetricWithPowerSource(const std::string& name,
                                   int sample,
                                   int min,
                                   int max,
                                   int buckets) {
    ExpectMetric(MetricsReporter::AppendPowerSourceToEnumName(
        name, metrics_reporter_.power_source()), sample, min, max, buckets);
  }

  // Adds a metrics library mock expectation that the specified enum
  // metric will be generated.
  void ExpectEnumMetric(const std::string& name,
                        int sample,
                        int max) {
    EXPECT_CALL(metrics_lib_, SendEnumToUMA(name, sample, max))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  void ExpectEnumMetricWithPowerSource(const std::string& name,
                                       int sample,
                                       int max) {
    ExpectEnumMetric(MetricsReporter::AppendPowerSourceToEnumName(
        name, metrics_reporter_.power_source()), sample, max);
  }

  // Adds a metrics library mock expectation for the battery discharge
  // rate metric with the given |sample|.
  void ExpectBatteryDischargeRateMetric(int sample) {
    ExpectMetric(kMetricBatteryDischargeRateName, sample,
                 kMetricBatteryDischargeRateMin,
                 kMetricBatteryDischargeRateMax,
                 kMetricBatteryDischargeRateBuckets);
  }

  // Adds a metrics library mock expectation for the remaining battery
  // info metric with the given |sample|.
  void ExpectBatteryInfoWhenChargeStartsMetric(int sample) {
    ExpectEnumMetric(kMetricBatteryRemainingWhenChargeStartsName, sample,
                     kMetricBatteryRemainingWhenChargeStartsMax);
    ExpectEnumMetric(kMetricBatteryChargeHealthName, sample,
                     kMetricBatteryChargeHealthMax);
  }

  // Adds a metrics library mock expectation for the remaining battery at end of
  // session metric with the given |sample|.
  void ExpectBatteryRemainingAtEndOfSessionMetric(int sample) {
    ExpectEnumMetricWithPowerSource(kMetricBatteryRemainingAtEndOfSessionName,
                                    sample,
                                    kMetricBatteryRemainingAtEndOfSessionMax);
  }

  // Adds a metrics library mock expectation for the remaining battery at start
  // of session metric with the given |sample|.
  void ExpectBatteryRemainingAtStartOfSessionMetric(int sample) {
    ExpectEnumMetricWithPowerSource(kMetricBatteryRemainingAtStartOfSessionName,
                                    sample,
                                    kMetricBatteryRemainingAtStartOfSessionMax);
  }

  // Adds a metrics library mock expectation for the number of ALS adjustments
  // per session metric with the given |sample|.
  void ExpectNumberOfAlsAdjustmentsPerSessionMetric(int sample) {
    ExpectMetric(kMetricNumberOfAlsAdjustmentsPerSessionName,
                 sample,
                 kMetricNumberOfAlsAdjustmentsPerSessionMin,
                 kMetricNumberOfAlsAdjustmentsPerSessionMax,
                 kMetricNumberOfAlsAdjustmentsPerSessionBuckets);
  }

  // Adds a metrics library mock expectation for the number of ALS adjustments
  // per session metric with the given |sample|.
  void ExpectUserBrightnessAdjustmentsPerSessionMetric(int sample) {
    ExpectMetricWithPowerSource(
        kMetricUserBrightnessAdjustmentsPerSessionName,
        sample,
        kMetricUserBrightnessAdjustmentsPerSessionMin,
        kMetricUserBrightnessAdjustmentsPerSessionMax,
        kMetricUserBrightnessAdjustmentsPerSessionBuckets);
  }

  // Adds a metrics library mock expectation for the length of session metric
  // with the given |sample|.
  void ExpectLengthOfSessionMetric(int sample) {
    ExpectMetric(kMetricLengthOfSessionName,
                 sample,
                 kMetricLengthOfSessionMin,
                 kMetricLengthOfSessionMax,
                 kMetricLengthOfSessionBuckets);
  }

  // Adds a metrics library mock expectation for the number of sessions per
  // charge metric with the given |sample|.
  void ExpectNumOfSessionsPerChargeMetric(int sample) {
    ExpectMetric(kMetricNumOfSessionsPerChargeName,
                 sample,
                 kMetricNumOfSessionsPerChargeMin,
                 kMetricNumOfSessionsPerChargeMax,
                 kMetricNumOfSessionsPerChargeBuckets);
  }

  // Adds metrics library mocks expectations for when a good sample has been
  // pulled and is being processed.
  void ExpectGoodBatteryInfoSample() {
    ExpectEnumMetric(kMetricBatteryInfoSampleName,
                     BATTERY_INFO_READ,
                     BATTERY_INFO_MAX);
    ExpectEnumMetric(kMetricBatteryInfoSampleName,
                     BATTERY_INFO_GOOD,
                     BATTERY_INFO_MAX);
  }

  // Adds metrics library mocks expectations for when a bad sample has been
  // pulled and is being processed.
  void ExpectBadBatteryInfoSample() {
    ExpectEnumMetric(kMetricBatteryInfoSampleName,
                     BATTERY_INFO_READ,
                     BATTERY_INFO_MAX);
    ExpectEnumMetric(kMetricBatteryInfoSampleName,
                     BATTERY_INFO_BAD,
                     BATTERY_INFO_MAX);
  }

  FakePrefs prefs_;
  BacklightControllerStub display_backlight_controller_;
  BacklightControllerStub keyboard_backlight_controller_;

  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;
  MetricsReporter metrics_reporter_;
};

TEST_F(MetricsReporterTest, CheckMetricInterval) {
  EXPECT_FALSE(MetricsReporter::CheckMetricInterval(29, 0, 30));
  EXPECT_TRUE(MetricsReporter::CheckMetricInterval(30, 0, 30));
  EXPECT_TRUE(MetricsReporter::CheckMetricInterval(29, 30, 100));
  EXPECT_FALSE(MetricsReporter::CheckMetricInterval(39, 30, 10));
  EXPECT_TRUE(MetricsReporter::CheckMetricInterval(40, 30, 10));
  EXPECT_TRUE(MetricsReporter::CheckMetricInterval(41, 30, 10));
}

TEST_F(MetricsReporterTest, GenerateBacklightLevelMetric) {
  metrics_reporter_.HandleScreenDimmedChange(true, base::TimeTicks::Now());
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  metrics_reporter_.GenerateBacklightLevelMetric();
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  const int64 kCurrentDisplayPercent = 57;
  display_backlight_controller_.set_percent(kCurrentDisplayPercent);
  const int64 kCurrentKeyboardPercent = 43;
  keyboard_backlight_controller_.set_percent(kCurrentKeyboardPercent);

  metrics_reporter_.HandleScreenDimmedChange(false, base::TimeTicks::Now());
  ExpectEnumMetric("Power.BacklightLevelOnBattery", kCurrentDisplayPercent,
                   kMetricBacklightLevelMax);
  ExpectEnumMetric("Power.KeyboardBacklightLevel", kCurrentKeyboardPercent,
                   kMetricKeyboardBacklightLevelMax);
  metrics_reporter_.GenerateBacklightLevelMetric();

  metrics_reporter_.HandlePowerSourceChange(POWER_AC);
  ExpectEnumMetric("Power.BacklightLevelOnAC", kCurrentDisplayPercent,
                   kMetricBacklightLevelMax);
  ExpectEnumMetric("Power.KeyboardBacklightLevel", kCurrentKeyboardPercent,
                   kMetricKeyboardBacklightLevelMax);
  metrics_reporter_.GenerateBacklightLevelMetric();
}

TEST_F(MetricsReporterTest, GenerateBatteryDischargeRateMetric) {
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  system::PowerStatus status;
  status.battery_energy_rate = 5.0;
  ExpectBatteryDischargeRateMetric(5000);
  EXPECT_TRUE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, kMetricBatteryDischargeRateInterval));

  status.battery_energy_rate = 4.5;
  ExpectBatteryDischargeRateMetric(4500);
  EXPECT_TRUE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, kMetricBatteryDischargeRateInterval - 1));

  status.battery_energy_rate = 6.4;
  ExpectBatteryDischargeRateMetric(6400);
  EXPECT_TRUE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, 2 * kMetricBatteryDischargeRateInterval));
}

TEST_F(MetricsReporterTest, GenerateBatteryDischargeRateMetricInterval) {
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  system::PowerStatus status;
  status.battery_energy_rate = 4.0;
  EXPECT_FALSE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, /* now */ 0));
  EXPECT_FALSE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, kMetricBatteryDischargeRateInterval - 1));
}

TEST_F(MetricsReporterTest, GenerateBatteryDischargeRateMetricNotDisconnected) {
  system::PowerStatus status;
  status.battery_energy_rate = 4.0;
  EXPECT_FALSE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, kMetricBatteryDischargeRateInterval));

  metrics_reporter_.HandlePowerSourceChange(POWER_AC);
  EXPECT_FALSE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, 2 * kMetricBatteryDischargeRateInterval));
}

TEST_F(MetricsReporterTest, GenerateBatteryDischargeRateMetricRateNonPositive) {
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  system::PowerStatus status;
  EXPECT_FALSE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, kMetricBatteryDischargeRateInterval));

  status.battery_energy_rate = -4.0;
  EXPECT_FALSE(metrics_reporter_.GenerateBatteryDischargeRateMetric(
      status, 2 * kMetricBatteryDischargeRateInterval));
}

TEST_F(MetricsReporterTest, GenerateBatteryInfoWhenChargeStartsMetric) {
  const double battery_percentages[] = { 10.1, 10.7,
                                         20.4, 21.6,
                                         60.4, 61.6,
                                         82.4, 82.5,
                                         102.4, 111.6};
  size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  system::PowerStatus status;
  status.battery_is_present = true;
  status.line_power_on = false;
  metrics_reporter_.GenerateBatteryInfoWhenChargeStartsMetric(status);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  status.battery_is_present = false;
  status.line_power_on = true;
  metrics_reporter_.GenerateBatteryInfoWhenChargeStartsMetric(status);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  status.battery_is_present = true;
  status.battery_charge_full_design = 100;
  for (size_t i = 0; i < num_percentages; i++) {
    status.battery_percentage = battery_percentages[i];
    status.battery_charge_full = battery_percentages[i];
    int expected_percentage = round(status.battery_percentage);

    ExpectBatteryInfoWhenChargeStartsMetric(expected_percentage);
    metrics_reporter_.GenerateBatteryInfoWhenChargeStartsMetric(status);
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(MetricsReporterTest, GenerateNumberOfAlsAdjustmentsPerSessionMetric) {
  static const uint adjustment_counts[] = {0, 100, 500, 1000};
  size_t num_counts = ARRAYSIZE_UNSAFE(adjustment_counts);

  for (size_t i = 0; i < num_counts; i++) {
    display_backlight_controller_.set_num_als_adjustments(adjustment_counts[i]);
    ExpectNumberOfAlsAdjustmentsPerSessionMetric(adjustment_counts[i]);
    EXPECT_TRUE(
        metrics_reporter_.GenerateNumberOfAlsAdjustmentsPerSessionMetric());
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(MetricsReporterTest,
       GenerateNumberOfAlsAdjustmentsPerSessionMetricOverflow) {
  display_backlight_controller_.set_num_als_adjustments(
      kMetricNumberOfAlsAdjustmentsPerSessionMax + kAdjustmentsOffset);
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(
      kMetricNumberOfAlsAdjustmentsPerSessionMax);
  EXPECT_TRUE(
      metrics_reporter_.GenerateNumberOfAlsAdjustmentsPerSessionMetric());
}

TEST_F(MetricsReporterTest,
       GenerateNumberOfAlsAdjustmentsPerSessionMetricUnderflow) {
  display_backlight_controller_.set_num_als_adjustments(-kAdjustmentsOffset);
  EXPECT_FALSE(
      metrics_reporter_.GenerateNumberOfAlsAdjustmentsPerSessionMetric());
}

TEST_F(MetricsReporterTest, GenerateLengthOfSessionMetric) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now - base::TimeDelta::FromSeconds(kSessionLength);

  ExpectLengthOfSessionMetric(kSessionLength);
  EXPECT_TRUE(metrics_reporter_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(MetricsReporterTest, GenerateLengthOfSessionMetricOverflow) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now - base::TimeDelta::FromSeconds(
      kMetricLengthOfSessionMax + kSessionLength);

  ExpectLengthOfSessionMetric(kMetricLengthOfSessionMax);
  EXPECT_TRUE(metrics_reporter_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(MetricsReporterTest, GenerateLengthOfSessionMetricUnderflow) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now + base::TimeDelta::FromSeconds(kSessionLength);

  EXPECT_FALSE(metrics_reporter_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(MetricsReporterTest, GenerateNumOfSessionsPerChargeMetric) {
  EXPECT_TRUE(metrics_reporter_.GenerateNumOfSessionsPerChargeMetric());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  metrics_reporter_.IncrementNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(1);
  EXPECT_TRUE(metrics_reporter_.GenerateNumOfSessionsPerChargeMetric());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  metrics_reporter_.IncrementNumOfSessionsPerChargeMetric();
  metrics_reporter_.IncrementNumOfSessionsPerChargeMetric();
  metrics_reporter_.IncrementNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(3);
  EXPECT_TRUE(metrics_reporter_.GenerateNumOfSessionsPerChargeMetric());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Check that the pref is used, so the count will persist across reboots.
  prefs_.SetInt64(kNumSessionsOnCurrentChargePref, 5);
  metrics_reporter_.IncrementNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(6);
  EXPECT_TRUE(metrics_reporter_.GenerateNumOfSessionsPerChargeMetric());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Negative values in the pref should be ignored.
  prefs_.SetInt64(kNumSessionsOnCurrentChargePref, -2);
  metrics_reporter_.IncrementNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(1);
  EXPECT_TRUE(metrics_reporter_.GenerateNumOfSessionsPerChargeMetric());
  Mock::VerifyAndClearExpectations(&metrics_lib_);
}

TEST_F(MetricsReporterTest, GenerateEndOfSessionMetrics) {
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  system::PowerStatus status;
  status.battery_percentage = 10.1;
  int expected_percentage = round(status.battery_percentage);
  ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);

  display_backlight_controller_.set_num_als_adjustments(kAdjustmentsOffset);
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(kAdjustmentsOffset);

  const int kNumUserAdjustments = 10;
  display_backlight_controller_.set_num_user_adjustments(kNumUserAdjustments);
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now - base::TimeDelta::FromSeconds(kSessionLength);
  ExpectLengthOfSessionMetric(kSessionLength);

  metrics_reporter_.GenerateEndOfSessionMetrics(status, now, start);
}

TEST_F(MetricsReporterTest, GenerateBatteryRemainingAtEndOfSessionMetric) {
  const double battery_percentages[] = {10.1, 10.7,
                                        20.4, 21.6,
                                        60.4, 61.6,
                                        82.4, 82.5};
  const size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  system::PowerStatus status;
  for (size_t i = 0; i < num_percentages; i++) {
    status.battery_percentage = battery_percentages[i];
    int expected_percentage = round(status.battery_percentage);

    metrics_reporter_.HandlePowerSourceChange(POWER_AC);
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_TRUE(metrics_reporter_.GenerateBatteryRemainingAtEndOfSessionMetric(
        status));

    metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_TRUE(metrics_reporter_.GenerateBatteryRemainingAtEndOfSessionMetric(
        status));
  }
}

TEST_F(MetricsReporterTest, GenerateBatteryRemainingAtStartOfSessionMetric) {
  const double battery_percentages[] = {10.1, 10.7,
                                        20.4, 21.6,
                                        60.4, 61.6,
                                        82.4, 82.5};
  const size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  system::PowerStatus status;
  for (size_t i = 0; i < num_percentages; i++) {
    status.battery_percentage = battery_percentages[i];
    int expected_percentage = round(status.battery_percentage);

    metrics_reporter_.HandlePowerSourceChange(POWER_AC);
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_TRUE(
        metrics_reporter_.GenerateBatteryRemainingAtStartOfSessionMetric(
            status));

    metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_TRUE(
        metrics_reporter_.GenerateBatteryRemainingAtStartOfSessionMetric(
            status));
  }
}

TEST_F(MetricsReporterTest, GenerateUserBrightnessAdjustmentsPerSessionMetric) {
  const int kNumUserAdjustments = 10;
  display_backlight_controller_.set_num_user_adjustments(kNumUserAdjustments);

  EXPECT_FALSE(
      metrics_reporter_.GenerateUserBrightnessAdjustmentsPerSessionMetric());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  metrics_reporter_.HandlePowerSourceChange(POWER_AC);
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_TRUE(
      metrics_reporter_.GenerateUserBrightnessAdjustmentsPerSessionMetric());

  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_TRUE(
      metrics_reporter_.GenerateUserBrightnessAdjustmentsPerSessionMetric());
}

TEST_F(MetricsReporterTest,
       GenerateUserBrightnessAdjustmentsPerSessionMetricOverflow) {
  display_backlight_controller_.set_num_user_adjustments(
      kMetricUserBrightnessAdjustmentsPerSessionMax + kAdjustmentsOffset);
  metrics_reporter_.HandlePowerSourceChange(POWER_AC);
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      kMetricUserBrightnessAdjustmentsPerSessionMax);
  EXPECT_TRUE(
      metrics_reporter_.GenerateUserBrightnessAdjustmentsPerSessionMetric());
}

TEST_F(MetricsReporterTest,
       GenerateUserBrightnessAdjustmentsPerSessionMetricUnderflow) {
  display_backlight_controller_.set_num_user_adjustments(-kAdjustmentsOffset);
  metrics_reporter_.HandlePowerSourceChange(POWER_AC);
  EXPECT_FALSE(
      metrics_reporter_.GenerateUserBrightnessAdjustmentsPerSessionMetric());
}

TEST_F(MetricsReporterTest, GenerateMetricsOnPowerEvent) {
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  system::PowerStatus status;
  status.battery_energy_rate = 4.9;
  status.battery_percentage = 32.5;
  status.battery_time_to_empty = base::TimeDelta::FromSeconds(10 * 60);
  ExpectBatteryDischargeRateMetric(4900);
  ExpectEnumMetric(kMetricBatteryInfoSampleName,
                   BATTERY_INFO_READ,
                   BATTERY_INFO_MAX);
  ExpectEnumMetric(kMetricBatteryInfoSampleName,
                   BATTERY_INFO_GOOD,
                   BATTERY_INFO_MAX);
  metrics_reporter_.GenerateMetricsOnPowerEvent(status);
}

TEST_F(MetricsReporterTest, SendEnumMetric) {
  ExpectEnumMetric("Dummy.EnumMetric", 50, 200);
  EXPECT_TRUE(metrics_reporter_.SendEnumMetric(
      "Dummy.EnumMetric", /* sample */ 50, /* max */ 200));
}

TEST_F(MetricsReporterTest, SendMetric) {
  ExpectMetric("Dummy.Metric", 3, 1, 100, 50);
  EXPECT_TRUE(metrics_reporter_.SendMetric("Dummy.Metric", /* sample */ 3,
      /* min */ 1, /* max */ 100, /* buckets */ 50));
}

TEST_F(MetricsReporterTest, SendMetricWithPowerSource) {
  EXPECT_FALSE(metrics_reporter_.SendMetricWithPowerSource("Dummy.Metric",
      /* sample */ 3, /* min */ 1, /* max */ 100, /* buckets */ 50));
  metrics_reporter_.HandlePowerSourceChange(POWER_BATTERY);
  ExpectMetric("Dummy.MetricOnBattery", 3, 1, 100, 50);
  EXPECT_TRUE(metrics_reporter_.SendMetricWithPowerSource("Dummy.Metric",
      /* sample */ 3, /* min */ 1, /* max */ 100, /* buckets */ 50));
  metrics_reporter_.HandlePowerSourceChange(POWER_AC);
  ExpectMetric("Dummy.MetricOnAC", 3, 1, 100, 50);
  EXPECT_TRUE(metrics_reporter_.SendMetricWithPowerSource("Dummy.Metric",
      /* sample */ 3, /* min */ 1, /* max */ 100, /* buckets */ 50));
}

TEST_F(MetricsReporterTest, PowerButtonDownMetric) {
  // We should ignore a button release that wasn't preceded by a press.
  metrics_reporter_.GeneratePowerButtonMetric(false, base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Presses that are followed by additional presses should also be ignored.
  metrics_reporter_.GeneratePowerButtonMetric(true, base::TimeTicks::Now());
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // We should ignore series of events with negative durations.
  const base::TimeTicks before_down_time = base::TimeTicks::Now();
  const base::TimeTicks down_time = before_down_time +
      base::TimeDelta::FromMilliseconds(kPowerButtonInterval);
  const base::TimeTicks up_time = down_time +
      base::TimeDelta::FromMilliseconds(kPowerButtonInterval);
  metrics_reporter_.GeneratePowerButtonMetric(true, down_time);
  metrics_reporter_.GeneratePowerButtonMetric(false, before_down_time);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Send a regular sequence of events and check that the duration is reported.
  metrics_reporter_.GeneratePowerButtonMetric(true, down_time);
  ExpectMetric(kMetricPowerButtonDownTimeName,
               (up_time - down_time).InMilliseconds(),
               kMetricPowerButtonDownTimeMin,
               kMetricPowerButtonDownTimeMax,
               kMetricPowerButtonDownTimeBuckets);
  metrics_reporter_.GeneratePowerButtonMetric(false, up_time);
}

TEST_F(MetricsReporterTest, BatteryDischargeRateWhileSuspended) {
  const double kEnergyBeforeSuspend = 60;
  const double kEnergyAfterResume = 50;

  const base::Time kSuspendTime = base::Time::FromInternalValue(1000);
  const base::Time kResumeTime = kSuspendTime + base::TimeDelta::FromHours(1);

  // We shouldn't send a sample if we haven't suspended.
  system::PowerStatus status;
  status.line_power_on = false;
  status.battery_energy = kEnergyAfterResume;
  EXPECT_FALSE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kResumeTime));
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // Ditto if the system is on AC before suspending...
  status.line_power_on = true;
  status.battery_energy = kEnergyBeforeSuspend;
  metrics_reporter_.PrepareForSuspend(status, kSuspendTime);
  metrics_reporter_.HandleResume();
  status.line_power_on = false;
  status.battery_energy = kEnergyAfterResume;
  EXPECT_FALSE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kResumeTime));
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // ... or after resuming...
  status.line_power_on = false;
  status.battery_energy = kEnergyBeforeSuspend;
  metrics_reporter_.PrepareForSuspend(status, kSuspendTime);
  metrics_reporter_.HandleResume();
  status.line_power_on = true;
  status.battery_energy = kEnergyAfterResume;
  EXPECT_FALSE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kResumeTime));
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // ... or if the battery's energy increased while the system was
  // suspended (i.e. it was temporarily connected to AC while suspended).
  status.line_power_on = false;
  status.battery_energy = kEnergyBeforeSuspend;
  metrics_reporter_.PrepareForSuspend(status, kSuspendTime);
  metrics_reporter_.HandleResume();
  status.battery_energy = kEnergyBeforeSuspend + 5.0;
  EXPECT_FALSE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kResumeTime));
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // The sample also shouldn't be reported if the system wasn't suspended
  // for very long.
  status.battery_energy = kEnergyBeforeSuspend;
  metrics_reporter_.PrepareForSuspend(status, kSuspendTime);
  metrics_reporter_.HandleResume();
  status.battery_energy = kEnergyAfterResume;
  const base::Time kShortResumeTime =
      kSuspendTime + base::TimeDelta::FromSeconds(
          kMetricBatteryDischargeRateWhileSuspendedMinSuspendSec - 1);
  EXPECT_FALSE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kShortResumeTime));
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // The sample should be reported if the energy decreased over a long
  // enough time.
  status.battery_energy = kEnergyBeforeSuspend;
  metrics_reporter_.PrepareForSuspend(status, kSuspendTime);
  metrics_reporter_.HandleResume();
  status.battery_energy = kEnergyAfterResume;
  int rate_mw = static_cast<int>(round(
      1000 * (kEnergyBeforeSuspend - kEnergyAfterResume) /
      ((kResumeTime - kSuspendTime).InSecondsF() / 3600)));
  ExpectMetric(kMetricBatteryDischargeRateWhileSuspendedName, rate_mw,
               kMetricBatteryDischargeRateWhileSuspendedMin,
               kMetricBatteryDischargeRateWhileSuspendedMax,
               kMetricBatteryDischargeRateWhileSuspendedBuckets);
  EXPECT_TRUE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kResumeTime));
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  // A subsequent call to the method without another suspend/resume cycle
  // shouldn't do anything, though.
  EXPECT_FALSE(
      metrics_reporter_.GenerateBatteryDischargeRateWhileSuspendedMetric(
          status, kResumeTime));
}

}  // namespace power_manager
