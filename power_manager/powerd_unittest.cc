// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::Test;

static const int64 kDefaultBrightness = 50;
static const int64 kMaxBrightness = 100;
static const int64 kPluggedBrightness = 70;
static const int64 kUnpluggedBrightness = 30;
static const int64 kAlsBrightness = 0;
static const int64 kSmallInterval = 500;
static const int64 kBigInterval = kSmallInterval * 4;
static const int64 kPluggedDim = kBigInterval;
static const int64 kPluggedOff = 2 * kBigInterval;
static const int64 kPluggedSuspend = 3 * kBigInterval;
static const int64 kUnpluggedDim = kPluggedDim;
static const int64 kUnpluggedOff = kPluggedOff;
static const int64 kUnpluggedSuspend = kPluggedSuspend;
static const int64 kPowerButtonInterval = 20;

bool CheckMetricInterval(time_t now, time_t last, time_t interval);

class DaemonTest : public Test {
 public:
  DaemonTest()
      : prefs_(FilePath("."), FilePath(".")),
        backlight_ctl_(&backlight_, &prefs_),
        daemon_(&backlight_ctl_, &prefs_, &metrics_lib_, &video_detector_,
                &audio_detector_, &monitor_reconfigure_, FilePath(".")) {}

  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor.
    EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
    EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
    EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                              SetArgumentPointee<1>(kMaxBrightness),
                              Return(true)));
    prefs_.SetInt64(kPluggedBrightnessOffset, kPluggedBrightness);
    prefs_.SetInt64(kUnpluggedBrightnessOffset, kUnpluggedBrightness);
    prefs_.SetInt64(kAlsBrightnessLevel, kAlsBrightness);
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

  // Resets all fields of |info| to 0.
  void ResetPowerStatus(PowerStatus& status) {
    memset(&status, 0, sizeof(PowerStatus));
  }

  StrictMock<MockBacklight> backlight_;
  StrictMock<MockVideoDetector> video_detector_;
  StrictMock<MockAudioDetector> audio_detector_;
  StrictMock<MockMonitorReconfigure> monitor_reconfigure_;
  PowerPrefs prefs_;
  PowerStatus status_;
  BacklightController backlight_ctl_;

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

TEST_F(DaemonTest, GenerateBatteryRemainingAtEndOfSessionMetric) {
  const double battery_percentages[] = {10.1, 10.7,
                                        20.4, 21.6,
                                        60.4, 61.6,
                                        82.4, 82.5};
  const int num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  for(int i = 0; i < num_percentages; i++) {
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
  }
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

  ExpectEnumMetric("Power.BacklightLevelOnBattery",
                   kDefaultBrightness, kMaxBrightness);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
  daemon_.plugged_state_ = kPowerConnected;
  ExpectEnumMetric("Power.BacklightLevelOnAC",
                   kDefaultBrightness, kMaxBrightness);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
}

TEST_F(DaemonTest, PowerButtonDownMetric) {
    EXPECT_FALSE(daemon_.OnPowerButtonUpMetric(base::Time::Now()));
    EXPECT_TRUE(daemon_.OnPowerButtonDownMetric(base::Time::Now()));

    const base::Time before_down_time = base::Time::Now();
    const base::Time down_time = before_down_time +
        base::TimeDelta::FromMilliseconds(kPowerButtonInterval);
    const base::Time up_time = down_time +
        base::TimeDelta::FromMilliseconds(kPowerButtonInterval);
    EXPECT_FALSE(daemon_.OnPowerButtonDownMetric(down_time));
    EXPECT_FALSE(daemon_.OnPowerButtonUpMetric(before_down_time));

    EXPECT_TRUE(daemon_.OnPowerButtonDownMetric(down_time));
    ExpectMetric(kMetricPowerButtonDownTimeName,
                 (up_time - down_time).InMilliseconds(),
                 kMetricPowerButtonDownTimeMin,
                 kMetricPowerButtonDownTimeMax,
                 kMetricPowerButtonDownTimeBuckets);
    EXPECT_TRUE(daemon_.OnPowerButtonUpMetric(up_time));
}

}  // namespace power_manager
