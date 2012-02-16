// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <gtest/gtest.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdint.h>
#include <X11/extensions/XTest.h>

#include "base/logging.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/metrics_constants.h"
#include "power_manager/mock_audio_detector.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/mock_monitor_reconfigure.h"
#include "power_manager/mock_video_detector.h"
#include "power_manager/power_constants.h"
#include "power_manager/powerd.h"

namespace power_manager {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::Test;

static const int64 kDefaultBrightnessLevel = 50;
static const int64 kMaxBrightnessLevel = 100;
static const double kPluggedBrightnessPercent = 70;
static const double kUnpluggedBrightnessPercent = 30;
static const int64 kSmallInterval = 500;
static const int64 kBigInterval = kSmallInterval * 4;
static const int64 kPluggedDim = kBigInterval;
static const int64 kPluggedOff = 2 * kBigInterval;
static const int64 kPluggedSuspend = 3 * kBigInterval;
static const int64 kUnpluggedDim = kPluggedDim;
static const int64 kUnpluggedOff = kPluggedOff;
static const int64 kUnpluggedSuspend = kPluggedSuspend;
static const int64 kPowerButtonInterval = 20;
static const int kSessionLength = 5;
static const int kAdjustmentsOffset = 100;

bool CheckMetricInterval(time_t now, time_t last, time_t interval);

class DaemonTest : public Test {
 public:
  DaemonTest()
      : prefs_(FilePath("."), FilePath(".")),
        backlight_ctl_(&backlight_, &prefs_),
        daemon_(&backlight_ctl_, &prefs_, &metrics_lib_, &video_detector_,
                &audio_detector_, &monitor_reconfigure_, &keylight_,
                FilePath(".")) {}

  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor.
    EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
    EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
    EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
    EXPECT_CALL(backlight_, GetCurrentBrightnessLevel(NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kDefaultBrightnessLevel),
                              Return(true)));
    EXPECT_CALL(backlight_, GetMaxBrightnessLevel(NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kMaxBrightnessLevel),
                              Return(true)));
    prefs_.SetDouble(kPluggedBrightnessOffset, kPluggedBrightnessPercent);
    prefs_.SetDouble(kUnpluggedBrightnessOffset, kUnpluggedBrightnessPercent);
    CHECK(backlight_ctl_.Init());
    ResetPowerStatus(status_);
  }

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

  // Adds a metrics library mock expectation that the specified enum
  // metric will be generated.
  void ExpectEnumMetric(const std::string& name, int sample, int max) {
    EXPECT_CALL(metrics_lib_, SendEnumToUMA(name, sample, max))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }
  void ExpectEnumMetric2(const std::string& name, int sample, int max) {
    EXPECT_CALL(metrics_lib_, SendEnumToUMA(name, sample, max))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  void ExpectMetricWithPowerState(const std::string& name,
                                  int sample,
                                  int min,
                                  int max,
                                  int buckets) {
    std::string name_with_power_state = name;
    if (daemon_.plugged_state_ == kPowerDisconnected) {
      name_with_power_state += "OnBattery";
    } else if (daemon_.plugged_state_ == kPowerConnected) {
      name_with_power_state += "OnAC";
    } else {
      return;
    }

    EXPECT_CALL(metrics_lib_, SendToUMA(name_with_power_state,
                                        sample,
                                        min,
                                        max,
                                        buckets))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  void ExpectEnumMetricWithPowerState(const std::string& name,
                                      int sample,
                                      int max) {
    std::string name_with_power_state = name;
    if (daemon_.plugged_state_ == kPowerDisconnected) {
      name_with_power_state += "OnBattery";
    } else if (daemon_.plugged_state_ == kPowerConnected) {
      name_with_power_state += "OnAC";
    } else {
      return;
    }

    EXPECT_CALL(metrics_lib_, SendEnumToUMA(name_with_power_state, sample, max))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
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
  // charge metric with the given |sample|.
  void ExpectBatteryRemainingChargeMetric(int sample) {
    ExpectEnumMetric(kMetricBatteryRemainingChargeName, sample,
                     kMetricBatteryRemainingChargeMax);
  }

  // Adds a metrics library mock expectation for the remaining battery
  // charge metric with the given |sample|.
  void ExpectBatteryRemainingWhenChargeStartsMetric(int sample) {
    ExpectEnumMetric(kMetricBatteryRemainingWhenChargeStartsName, sample,
                     kMetricBatteryRemainingWhenChargeStartsMax);
  }

  // Adds a metrics library mock expectation for the battery's
  // remaining to empty metric with the given |sample|.
  void ExpectBatteryTimeToEmptyMetric(int sample) {
    ExpectMetric(kMetricBatteryTimeToEmptyName, sample,
                 kMetricBatteryTimeToEmptyMin,
                 kMetricBatteryTimeToEmptyMax,
                 kMetricBatteryTimeToEmptyBuckets);
  }

  // Adds a metrics library mock expectation for the remaining battery at end of
  // session metric with the given |sample|.
  void ExpectBatteryRemainingAtEndOfSessionMetric(int sample) {
    ExpectEnumMetricWithPowerState(kMetricBatteryRemainingAtEndOfSessionName,
                                   sample,
                                   kMetricBatteryRemainingAtEndOfSessionMax);
  }

  // Adds a metrics library mock expectation for the remaining battery at start
  // of session metric with the given |sample|.
  void ExpectBatteryRemainingAtStartOfSessionMetric(int sample) {
    ExpectEnumMetricWithPowerState(kMetricBatteryRemainingAtStartOfSessionName,
                                   sample,
                                   kMetricBatteryRemainingAtStartOfSessionMax);
  }

  // Resets all fields of |info| to 0.
  void ResetPowerStatus(PowerStatus& status) {
    memset(&status, 0, sizeof(PowerStatus));
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
    ExpectMetricWithPowerState(
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

  StrictMock<MockBacklight> backlight_;
  StrictMock<MockVideoDetector> video_detector_;
  StrictMock<MockAudioDetector> audio_detector_;
  StrictMock<MockMonitorReconfigure> monitor_reconfigure_;
  PluggedState plugged_state_;
  PowerPrefs prefs_;
  PowerStatus status_;
  BacklightController backlight_ctl_;
  StrictMock<MockBacklight> keylight_;

  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;
  Daemon daemon_;
};

TEST_F(DaemonTest, CheckMetricInterval) {
  EXPECT_FALSE(CheckMetricInterval(29, 0, 30));
  EXPECT_TRUE(CheckMetricInterval(30, 0, 30));
  EXPECT_TRUE(CheckMetricInterval(29, 30, 100));
  EXPECT_FALSE(CheckMetricInterval(39, 30, 10));
  EXPECT_TRUE(CheckMetricInterval(40, 30, 10));
  EXPECT_TRUE(CheckMetricInterval(41, 30, 10));
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetric) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_energy_rate = 5.0;
  ExpectBatteryDischargeRateMetric(5000);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(kMetricBatteryDischargeRateInterval,
            daemon_.battery_discharge_rate_metric_last_);

  status_.battery_energy_rate = 4.5;
  ExpectBatteryDischargeRateMetric(4500);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, kMetricBatteryDischargeRateInterval - 1));
  EXPECT_EQ(kMetricBatteryDischargeRateInterval - 1,
            daemon_.battery_discharge_rate_metric_last_);

  status_.battery_energy_rate = 6.4;
  ExpectBatteryDischargeRateMetric(6400);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, 2 * kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(2 * kMetricBatteryDischargeRateInterval,
            daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricInterval) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(status_,
                                                          /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, kMetricBatteryDischargeRateInterval - 1));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected) {
  EXPECT_EQ(kPowerUnknown, daemon_.plugged_state_);

  status_.battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, 2 * kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive) {
  daemon_.plugged_state_ = kPowerDisconnected;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  status_.battery_energy_rate = -4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, 2 * kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingChargeMetric) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_percentage = 10.4;
  ExpectBatteryRemainingChargeMetric(10);
  EXPECT_TRUE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(kMetricBatteryRemainingChargeInterval,
            daemon_.battery_remaining_charge_metric_last_);

  status_.battery_percentage = 11.6;
  ExpectBatteryRemainingChargeMetric(12);
  EXPECT_TRUE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, kMetricBatteryRemainingChargeInterval - 1));
  EXPECT_EQ(kMetricBatteryRemainingChargeInterval - 1,
            daemon_.battery_remaining_charge_metric_last_);

  status_.battery_percentage = 14.5;
  ExpectBatteryRemainingChargeMetric(15);
  EXPECT_TRUE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, 2 * kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(2 * kMetricBatteryRemainingChargeInterval,
            daemon_.battery_remaining_charge_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingChargeMetricInterval) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_percentage = 13.0;
  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(status_,
                                                            /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, kMetricBatteryRemainingChargeInterval - 1));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingChargeMetricNotDisconnected) {
  EXPECT_EQ(kPowerUnknown, daemon_.plugged_state_);

  status_.battery_percentage = 20.0;
  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);

  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, 2 * kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingWhenChargeStartsMetric) {
  const double battery_percentages[] = { 10.1, 10.7,
                                         20.4, 21.6,
                                         60.4, 61.6,
                                         82.4, 82.5 };
  size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  status_.battery_is_present = true;
  plugged_state_ = kPowerDisconnected;
  daemon_.GenerateBatteryRemainingWhenChargeStartsMetric(plugged_state_,
                                                         status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  plugged_state_ = kPowerUnknown;
  daemon_.GenerateBatteryRemainingWhenChargeStartsMetric(plugged_state_,
                                                         status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  status_.battery_is_present = false;
  plugged_state_ = kPowerConnected;
  daemon_.GenerateBatteryRemainingWhenChargeStartsMetric(plugged_state_,
                                                         status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  status_.battery_is_present = true;
  for(size_t i = 0; i < num_percentages; i++) {
    status_.battery_percentage = battery_percentages[i];
    int expected_percentage = round(status_.battery_percentage);

    ExpectBatteryRemainingWhenChargeStartsMetric(expected_percentage);
    daemon_.GenerateBatteryRemainingWhenChargeStartsMetric(plugged_state_,
                                                           status_);
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateBatteryTimeToEmptyMetric) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_time_to_empty = 90;
  ExpectBatteryTimeToEmptyMetric(2);
  EXPECT_TRUE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(kMetricBatteryTimeToEmptyInterval,
            daemon_.battery_time_to_empty_metric_last_);

  status_.battery_time_to_empty = 89;
  ExpectBatteryTimeToEmptyMetric(1);
  EXPECT_TRUE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, kMetricBatteryTimeToEmptyInterval - 1));
  EXPECT_EQ(kMetricBatteryTimeToEmptyInterval - 1,
            daemon_.battery_time_to_empty_metric_last_);

  status_.battery_time_to_empty = 151;
  ExpectBatteryTimeToEmptyMetric(3);
  EXPECT_TRUE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, 2 * kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(2 * kMetricBatteryTimeToEmptyInterval,
            daemon_.battery_time_to_empty_metric_last_);
}

TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetric) {
  static const uint adjustment_counts[] = {0, 100, 500, 1000};
  size_t num_counts = ARRAYSIZE_UNSAFE(adjustment_counts);

  for(size_t i = 0; i < num_counts; i++) {
    backlight_ctl_.als_adjustment_count_ = adjustment_counts[i];
    ExpectNumberOfAlsAdjustmentsPerSessionMetric(adjustment_counts[i]);
    EXPECT_TRUE(
        daemon_.GenerateNumberOfAlsAdjustmentsPerSessionMetric(
            backlight_ctl_));
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetricOverflow) {
  backlight_ctl_.als_adjustment_count_ =
      kMetricNumberOfAlsAdjustmentsPerSessionMax + kAdjustmentsOffset;
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(
      kMetricNumberOfAlsAdjustmentsPerSessionMax);
  EXPECT_TRUE(
      daemon_.GenerateNumberOfAlsAdjustmentsPerSessionMetric(
          backlight_ctl_));
}

TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetricUnderflow) {
  backlight_ctl_.als_adjustment_count_ = -kAdjustmentsOffset;
  EXPECT_FALSE(daemon_.GenerateNumberOfAlsAdjustmentsPerSessionMetric(
            backlight_ctl_));
}

TEST_F(DaemonTest, GenerateLengthOfSessionMetric) {
  base::Time now = base::Time::Now();
  base::Time start = now - base::TimeDelta::FromSeconds(kSessionLength);

  ExpectLengthOfSessionMetric(kSessionLength);
  EXPECT_TRUE(daemon_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(DaemonTest, GenerateLengthOfSessionMetricOverflow) {
  base::Time now = base::Time::Now();
  base::Time start = now - base::TimeDelta::FromSeconds(
      kMetricLengthOfSessionMax + kSessionLength);

  ExpectLengthOfSessionMetric(kMetricLengthOfSessionMax);
  EXPECT_TRUE(daemon_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(DaemonTest, GenerateLengthOfSessionMetricUnderflow) {
  base::Time now = base::Time::Now();
  base::Time start = now + base::TimeDelta::FromSeconds(kSessionLength);

  EXPECT_FALSE(daemon_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(DaemonTest, GenerateBatteryTimeToEmptyMetricInterval) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_time_to_empty = 100;
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(status_, /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, kMetricBatteryTimeToEmptyInterval - 1));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryTimeToEmptyMetricNotDisconnected) {
  EXPECT_EQ(kPowerUnknown, daemon_.plugged_state_);

  status_.battery_time_to_empty = 120;
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);

  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, 2 * kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
}

TEST_F(DaemonTest, GenerateEndOfSessionMetrics) {
  status_.battery_percentage = 10.1;
  int expected_percentage = round(status_.battery_percentage);
  ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);

  backlight_ctl_.als_adjustment_count_ = kAdjustmentsOffset;
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(
      backlight_ctl_.als_adjustment_count_);

  backlight_ctl_.user_adjustment_count_ = kAdjustmentsOffset;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_.user_adjustment_count_);

  base::Time now = base::Time::Now();
  base::Time start = now - base::TimeDelta::FromSeconds(kSessionLength);
  ExpectLengthOfSessionMetric(kSessionLength);

  daemon_.GenerateEndOfSessionMetrics(status_,
                                      backlight_ctl_,
                                      now,
                                      start);
}

TEST_F(DaemonTest, GenerateBatteryRemainingAtEndOfSessionMetric) {
  const double battery_percentages[] = {10.1, 10.7,
                                        20.4, 21.6,
                                        60.4, 61.6,
                                        82.4, 82.5};
  const size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  for(size_t i = 0; i < num_percentages; i++) {
    status_.battery_percentage = battery_percentages[i];
    int expected_percentage = round(status_.battery_percentage);

    daemon_.plugged_state_ = kPowerConnected;
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtEndOfSessionMetric(
        status_));

    daemon_.plugged_state_ = kPowerDisconnected;
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtEndOfSessionMetric(
        status_));

    daemon_.plugged_state_ = kPowerUnknown;
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_FALSE(daemon_.GenerateBatteryRemainingAtEndOfSessionMetric(
        status_));
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateBatteryRemainingAtStartOfSessionMetric) {
  const double battery_percentages[] = {10.1, 10.7,
                                        20.4, 21.6,
                                        60.4, 61.6,
                                        82.4, 82.5};
  const size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  for(size_t i = 0; i < num_percentages; i++) {
    status_.battery_percentage = battery_percentages[i];
    int expected_percentage = round(status_.battery_percentage);

    daemon_.plugged_state_ = kPowerConnected;
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtStartOfSessionMetric(
        status_));

    daemon_.plugged_state_ = kPowerDisconnected;
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtStartOfSessionMetric(
        status_));

    daemon_.plugged_state_ = kPowerUnknown;
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_FALSE(daemon_.GenerateBatteryRemainingAtStartOfSessionMetric(
        status_));
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetric) {
  backlight_ctl_.user_adjustment_count_ = kAdjustmentsOffset;

  daemon_.plugged_state_ = kPowerConnected;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_.user_adjustment_count_);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));

  daemon_.plugged_state_ = kPowerDisconnected;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_.user_adjustment_count_);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));

  daemon_.plugged_state_ = kPowerUnknown;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_.user_adjustment_count_);
  EXPECT_FALSE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

TEST_F(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetricOverflow) {
  backlight_ctl_.user_adjustment_count_ =
      kMetricUserBrightnessAdjustmentsPerSessionMax + kAdjustmentsOffset;
  daemon_.plugged_state_ = kPowerConnected;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      kMetricUserBrightnessAdjustmentsPerSessionMax);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

TEST_F(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetricUnderflow) {
  backlight_ctl_.user_adjustment_count_ = -kAdjustmentsOffset;
  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

TEST_F(DaemonTest, GenerateMetricsOnPowerEvent) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_energy_rate = 4.9;
  status_.battery_percentage = 32.5;
  status_.battery_time_to_empty = 10 * 60;
  ExpectBatteryDischargeRateMetric(4900);
  ExpectBatteryRemainingChargeMetric(33);
  ExpectBatteryTimeToEmptyMetric(10);
  daemon_.GenerateMetricsOnPowerEvent(status_);
  EXPECT_LT(0, daemon_.battery_discharge_rate_metric_last_);
  EXPECT_LT(0, daemon_.battery_remaining_charge_metric_last_);
  EXPECT_LT(0, daemon_.battery_time_to_empty_metric_last_);
}

TEST_F(DaemonTest, SendEnumMetric) {
  ExpectEnumMetric("Dummy.EnumMetric", 50, 200);
  EXPECT_TRUE(daemon_.SendEnumMetric("Dummy.EnumMetric", /* sample */ 50,
                                     /* max */ 200));
}

TEST_F(DaemonTest, SendMetric) {
  ExpectMetric("Dummy.Metric", 3, 1, 100, 50);
  EXPECT_TRUE(daemon_.SendMetric("Dummy.Metric", /* sample */ 3,
                                 /* min */ 1, /* max */ 100, /* buckets */ 50));
}

TEST_F(DaemonTest, SendMetricWithPowerState) {
  EXPECT_FALSE(daemon_.SendMetricWithPowerState("Dummy.Metric", /* sample */ 3,
      /* min */ 1, /* max */ 100, /* buckets */ 50));
  daemon_.plugged_state_ = kPowerDisconnected;
  ExpectMetric("Dummy.MetricOnBattery", 3, 1, 100, 50);
  EXPECT_TRUE(daemon_.SendMetricWithPowerState("Dummy.Metric", /* sample */ 3,
      /* min */ 1, /* max */ 100, /* buckets */ 50));
  daemon_.plugged_state_ = kPowerConnected;
  ExpectMetric("Dummy.MetricOnAC", 3, 1, 100, 50);
  EXPECT_TRUE(daemon_.SendMetricWithPowerState("Dummy.Metric", /* sample */ 3,
      /* min */ 1, /* max */ 100, /* buckets */ 50));
}

TEST_F(DaemonTest, GenerateBacklightLevelMetric) {
  daemon_.plugged_state_ = kPowerDisconnected;
  daemon_.SetPlugged(kPowerDisconnected);
  daemon_.backlight_controller_->OnPlugEvent(kPowerDisconnected);
  daemon_.backlight_controller_->SetPowerState(BACKLIGHT_DIM);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
  daemon_.backlight_controller_->SetPowerState(BACKLIGHT_ACTIVE);
  daemon_.plugged_state_ = kPowerDisconnected;

  int64 default_brightness_percent = static_cast<int64>(lround(
      daemon_.backlight_controller_->LevelToPercent(kDefaultBrightnessLevel)));

  ExpectEnumMetric("Power.BacklightLevelOnBattery",
                   default_brightness_percent,
                   kMetricBacklightLevelMax);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
  daemon_.plugged_state_ = kPowerConnected;
  ExpectEnumMetric("Power.BacklightLevelOnAC",
                   default_brightness_percent,
                   kMetricBacklightLevelMax);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
}

TEST_F(DaemonTest, PowerButtonDownMetric) {
  // We should ignore a button release that wasn't preceded by a press.
  daemon_.SendPowerButtonMetric(false, base::TimeTicks::Now());

  // Presses that are followed by additional presses should also be ignored.
  daemon_.SendPowerButtonMetric(true, base::TimeTicks::Now());

  // We should ignore series of events with negative durations.
  const base::TimeTicks before_down_time = base::TimeTicks::Now();
  const base::TimeTicks down_time = before_down_time +
      base::TimeDelta::FromMilliseconds(kPowerButtonInterval);
  const base::TimeTicks up_time = down_time +
      base::TimeDelta::FromMilliseconds(kPowerButtonInterval);
  daemon_.SendPowerButtonMetric(true, down_time);
  daemon_.SendPowerButtonMetric(false, before_down_time);

  // Send a regular sequence of events and check that the duration is reported.
  daemon_.SendPowerButtonMetric(true, down_time);
  ExpectMetric(kMetricPowerButtonDownTimeName,
               (up_time - down_time).InMilliseconds(),
               kMetricPowerButtonDownTimeMin,
               kMetricPowerButtonDownTimeMax,
               kMetricPowerButtonDownTimeBuckets);
  daemon_.SendPowerButtonMetric(false, up_time);
}

}  // namespace power_manager
