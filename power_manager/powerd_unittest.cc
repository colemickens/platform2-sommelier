// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <X11/extensions/XTest.h>

#include <cmath>

#include "base/logging.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/metrics_constants.h"
#include "power_manager/mock_activity_detector.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/mock_metrics_store.h"
#include "power_manager/mock_monitor_reconfigure.h"
#include "power_manager/mock_rolling_average.h"
#include "power_manager/mock_xsync.h"
#include "power_manager/power_constants.h"
#include "power_manager/powerd.h"

#ifdef IS_DESKTOP
#include "power_manager/external_backlight_controller.h"
#else
#include "power_manager/internal_backlight_controller.h"
#endif

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
static const int kNumOfSessionsPerCharge = 100;
static const int64 kBatteryTime = 23;

bool CheckMetricInterval(time_t now, time_t last, time_t interval);

class DaemonTest : public Test {
 public:
  DaemonTest()
      : prefs_(FilePath("."), FilePath(".")),
#ifdef IS_DESKTOP
        backlight_ctl_(&backlight_),
#else
        backlight_ctl_(&backlight_, &prefs_),
#endif
        idle_(new MockXSync),
        daemon_(&backlight_ctl_, &prefs_, &metrics_lib_, &video_detector_,
                &audio_detector_, &idle_, &monitor_reconfigure_, &keylight_,
                FilePath(".")) {}

  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor.
    EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
    EXPECT_CALL(backlight_, GetCurrentBrightnessLevel(NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kDefaultBrightnessLevel),
                              Return(true)));
    EXPECT_CALL(backlight_, GetMaxBrightnessLevel(NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kMaxBrightnessLevel),
                              Return(true)));
    EXPECT_CALL(backlight_, SetBrightnessLevel(_))
        .WillRepeatedly(Return(true));
    prefs_.SetDouble(kPluggedBrightnessOffset, kPluggedBrightnessPercent);
    prefs_.SetDouble(kUnpluggedBrightnessOffset, kUnpluggedBrightnessPercent);
#ifdef IS_DESKTOP
    backlight_ctl_.set_disable_dbus_for_testing(true);
#endif
    CHECK(backlight_ctl_.Init());
    ResetPowerStatus(&status_);
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
  void ExpectBatteryRemainingWhenChargeStartsMetric(int sample) {
    ExpectEnumMetric(kMetricBatteryRemainingWhenChargeStartsName, sample,
                     kMetricBatteryRemainingWhenChargeStartsMax);
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

  // Resets all fields of |status| to 0.
  void ResetPowerStatus(PowerStatus* status) {
    memset(status, 0, sizeof(PowerStatus));
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

  // Adds a metrics library mock expectation for the number of sessions per
  // charge metric with the given |sample|.
  void ExpectNumOfSessionsPerChargeMetric(int sample) {
    ExpectMetric(kMetricNumOfSessionsPerChargeName,
                 sample,
                 kMetricNumOfSessionsPerChargeMin,
                 kMetricNumOfSessionsPerChargeMax,
                 kMetricNumOfSessionsPerChargeBuckets);
  }

  StrictMock<MockBacklight> backlight_;
  StrictMock<MockActivityDetector> video_detector_;
  StrictMock<MockActivityDetector> audio_detector_;
  StrictMock<MockMonitorReconfigure> monitor_reconfigure_;
  StrictMock<MockMetricsStore> metrics_store_;
  PluggedState plugged_state_;
  PowerPrefs prefs_;
  PowerStatus status_;
#ifdef IS_DESKTOP
  ExternalBacklightController backlight_ctl_;
#else
  InternalBacklightController backlight_ctl_;
#endif
  StrictMock<MockBacklight> keylight_;

  StrictMock<MockRollingAverage> empty_average_;
  StrictMock<MockRollingAverage> full_average_;

  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;
  XIdle idle_;
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
  for (size_t i = 0; i < num_percentages; i++) {
    status_.battery_percentage = battery_percentages[i];
    int expected_percentage = round(status_.battery_percentage);

    ExpectBatteryRemainingWhenChargeStartsMetric(expected_percentage);
    daemon_.GenerateBatteryRemainingWhenChargeStartsMetric(plugged_state_,
                                                           status_);
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

#ifndef IS_DESKTOP
TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetric) {
  static const uint adjustment_counts[] = {0, 100, 500, 1000};
  size_t num_counts = ARRAYSIZE_UNSAFE(adjustment_counts);

  for (size_t i = 0; i < num_counts; i++) {
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
#endif  // !IS_DESKTOP

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

TEST_F(DaemonTest, GenerateNumOfSessionsPerChargeMetric) {
  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(0);
  EXPECT_TRUE(daemon_.GenerateNumOfSessionsPerChargeMetric(&metrics_store_));
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(
      kNumOfSessionsPerCharge);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(kNumOfSessionsPerCharge);
  EXPECT_TRUE(daemon_.GenerateNumOfSessionsPerChargeMetric(&metrics_store_));
  Mock::VerifyAndClearExpectations(&metrics_lib_);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  EXPECT_DEATH(daemon_.GenerateNumOfSessionsPerChargeMetric(NULL), ".*");
}

TEST_F(DaemonTest, HandleNumOfSessionsPerChargeOnSetPlugged) {
  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(
      kNumOfSessionsPerCharge);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(kNumOfSessionsPerCharge);
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   kPowerConnected);
  Mock::VerifyAndClearExpectations(&metrics_lib_);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(0);
  metrics_store_.ExpectIncrementNumOfSessionsPerChargeMetric();
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   kPowerDisconnected);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(1);
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   kPowerDisconnected);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(
      kNumOfSessionsPerCharge);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  metrics_store_.ExpectIncrementNumOfSessionsPerChargeMetric();
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   kPowerDisconnected);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(-1);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  metrics_store_.ExpectIncrementNumOfSessionsPerChargeMetric();
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   kPowerDisconnected);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   kPowerUnknown);

  EXPECT_DEATH(daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(
      NULL,
      kPowerConnected),
               ".*");
  EXPECT_DEATH(daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(
      NULL,
      kPowerDisconnected),
               ".*");
  EXPECT_DEATH(daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(
      NULL,
      kPowerUnknown),
               ".*");
}

TEST_F(DaemonTest, GenerateEndOfSessionMetrics) {
  status_.battery_percentage = 10.1;
  int expected_percentage = round(status_.battery_percentage);
  ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);

#ifndef IS_DESKTOP
  backlight_ctl_.als_adjustment_count_ = kAdjustmentsOffset;
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(
      backlight_ctl_.als_adjustment_count_);
#else
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(0);
#endif

  const int kNumUserAdjustments = 10;
  for (int i = 0; i < kNumUserAdjustments; ++i)
    backlight_ctl_.IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED);
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);

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

  for (size_t i = 0; i < num_percentages; i++) {
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

  for (size_t i = 0; i < num_percentages; i++) {
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
  const int kNumUserAdjustments = 10;
  for (int i = 0; i < kNumUserAdjustments; ++i)
    backlight_ctl_.IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED);

  daemon_.plugged_state_ = kPowerConnected;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));

  daemon_.plugged_state_ = kPowerDisconnected;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));

  daemon_.plugged_state_ = kPowerUnknown;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_FALSE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

#ifndef IS_DESKTOP
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
#endif

TEST_F(DaemonTest, GenerateMetricsOnPowerEvent) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_energy_rate = 4.9;
  status_.battery_percentage = 32.5;
  status_.battery_time_to_empty = 10 * 60;
  ExpectBatteryDischargeRateMetric(4900);
  daemon_.GenerateMetricsOnPowerEvent(status_);
  EXPECT_LT(0, daemon_.battery_discharge_rate_metric_last_);
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

  double current_percent = 0.0;
  ASSERT_TRUE(
      daemon_.backlight_controller_->GetCurrentBrightnessPercent(
          &current_percent));
  int64 current_percent_int = static_cast<int64>(lround(current_percent));

  ExpectEnumMetric("Power.BacklightLevelOnBattery",
                   current_percent_int,
                   kMetricBacklightLevelMax);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
  daemon_.plugged_state_ = kPowerConnected;
  ExpectEnumMetric("Power.BacklightLevelOnAC",
                   current_percent_int,
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

TEST_F(DaemonTest, ExtendTimeoutsWhenProjecting) {
  const int64 kPluggedDimTimeMs = 10000;
  const int64 kPluggedOffTimeMs = 20000;
  const int64 kPluggedSuspendTimeMs = 40000;
  const int64 kUnpluggedDimTimeMs = 15000;
  const int64 kUnpluggedOffTimeMs = 25000;
  const int64 kUnpluggedSuspendTimeMs = 45000;

  const int64 kLockTimeMs = 30000;

  // Set prefs that are read by ReadSettings().  Use 0 for ones that we don't
  // care about.
  prefs_.SetInt64(kLowBatterySuspendPercent, 0);
  prefs_.SetInt64(kCleanShutdownTimeoutMs, 0);
  prefs_.SetInt64(kPluggedDimMs, kPluggedDimTimeMs);
  prefs_.SetInt64(kPluggedOffMs, kPluggedOffTimeMs);
  prefs_.SetInt64(kPluggedSuspendMs, kPluggedSuspendTimeMs);
  prefs_.SetInt64(kUnpluggedDimMs, kUnpluggedDimTimeMs);
  prefs_.SetInt64(kUnpluggedOffMs, kUnpluggedOffTimeMs);
  prefs_.SetInt64(kUnpluggedSuspendMs, kUnpluggedSuspendTimeMs);
  prefs_.SetInt64(kReactMs, 0);
  prefs_.SetInt64(kFuzzMs, 0);
  prefs_.SetInt64(kEnforceLock, 0);
  prefs_.SetInt64(kUseXScreenSaver, 0);
  prefs_.SetInt64(kDisableIdleSuspend, 0);
  prefs_.SetInt64(kLockOnIdleSuspend, 1);
  prefs_.SetInt64(kLockMs, kLockTimeMs);

  // Check that the settings are loaded correctly.
  EXPECT_CALL(monitor_reconfigure_, is_projecting())
      .WillRepeatedly(Return(false));
  daemon_.ReadSettings();
  EXPECT_EQ(kPluggedDimTimeMs, daemon_.plugged_dim_ms_);
  EXPECT_EQ(kPluggedOffTimeMs, daemon_.plugged_off_ms_);
  EXPECT_EQ(kPluggedSuspendTimeMs, daemon_.plugged_suspend_ms_);
  EXPECT_EQ(kUnpluggedDimTimeMs, daemon_.unplugged_dim_ms_);
  EXPECT_EQ(kUnpluggedOffTimeMs, daemon_.unplugged_off_ms_);
  EXPECT_EQ(kUnpluggedSuspendTimeMs, daemon_.unplugged_suspend_ms_);
  EXPECT_EQ(kLockTimeMs, daemon_.default_lock_ms_);

  // When we start projecting, all of the timeouts should be increased.
  EXPECT_CALL(monitor_reconfigure_, is_projecting())
      .WillRepeatedly(Return(true));
  daemon_.AdjustIdleTimeoutsForProjection();
  EXPECT_GT(daemon_.plugged_dim_ms_, kPluggedDimTimeMs);
  EXPECT_GT(daemon_.plugged_off_ms_, kPluggedOffTimeMs);
  EXPECT_GT(daemon_.plugged_suspend_ms_, kPluggedSuspendTimeMs);
  EXPECT_GT(daemon_.unplugged_dim_ms_, kUnpluggedDimTimeMs);
  EXPECT_GT(daemon_.unplugged_off_ms_, kUnpluggedOffTimeMs);
  EXPECT_GT(daemon_.unplugged_suspend_ms_, kUnpluggedSuspendTimeMs);
  EXPECT_GT(daemon_.default_lock_ms_, kLockTimeMs);

  // Check that the lock timeout remains higher than the screen-off timeout
  // (http://crosbug.com/24847).
  EXPECT_GT(daemon_.default_lock_ms_, daemon_.plugged_off_ms_);

  // Stop projecting and check that we go back to the previous values.
  EXPECT_CALL(monitor_reconfigure_, is_projecting())
      .WillRepeatedly(Return(false));
  daemon_.AdjustIdleTimeoutsForProjection();
  EXPECT_EQ(kPluggedDimTimeMs, daemon_.plugged_dim_ms_);
  EXPECT_EQ(kPluggedOffTimeMs, daemon_.plugged_off_ms_);
  EXPECT_EQ(kPluggedSuspendTimeMs, daemon_.plugged_suspend_ms_);
  EXPECT_EQ(kUnpluggedDimTimeMs, daemon_.unplugged_dim_ms_);
  EXPECT_EQ(kUnpluggedOffTimeMs, daemon_.unplugged_off_ms_);
  EXPECT_EQ(kUnpluggedSuspendTimeMs, daemon_.unplugged_suspend_ms_);
  EXPECT_EQ(kLockTimeMs, daemon_.default_lock_ms_);
}

TEST_F(DaemonTest, UpdateAveragedTimesChargingAndCalculating) {
  status_.line_power_on = true;
  status_.is_calculating_battery_time = true;

  empty_average_.ExpectClear();
  full_average_.ExpectGetAverage(kBatteryTime);

  daemon_.UpdateAveragedTimes(&status_, &empty_average_, &full_average_);

  EXPECT_EQ(0, status_.averaged_battery_time_to_empty);
  EXPECT_EQ(kBatteryTime, status_.averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesChargingAndNotCalculating) {
  status_.line_power_on = true;
  status_.is_calculating_battery_time = false;
  status_.battery_time_to_full = kBatteryTime;

  empty_average_.ExpectClear();
  full_average_.ExpectAddSample(kBatteryTime, kBatteryTime);

  daemon_.UpdateAveragedTimes(&status_, &empty_average_, &full_average_);

  EXPECT_EQ(0, status_.averaged_battery_time_to_empty);
  EXPECT_EQ(kBatteryTime, status_.averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesDischargingAndCalculating) {
  status_.line_power_on = false;
  status_.is_calculating_battery_time = true;

  empty_average_.ExpectGetAverage(kBatteryTime);
  full_average_.ExpectClear();

  daemon_.UpdateAveragedTimes(&status_, &empty_average_, &full_average_);

  EXPECT_EQ(kBatteryTime, status_.averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_.averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesDischargingAndNotCalculating) {
  status_.line_power_on = false;
  status_.is_calculating_battery_time = false;
  status_.battery_time_to_empty = kBatteryTime;

  empty_average_.ExpectAddSample(kBatteryTime, kBatteryTime);
  full_average_.ExpectClear();

  daemon_.UpdateAveragedTimes(&status_, &empty_average_, &full_average_);

  EXPECT_EQ(kBatteryTime, status_.averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_.averaged_battery_time_to_full);
}

TEST_F(DaemonTest, SendThermalMetrics) {
  int aborted = 5;
  int turned_on = 10;
  int multiple = 2;
  int total = aborted + turned_on;

  ExpectEnumMetric(kMetricThermalAbortedFanTurnOnName,
                   static_cast<int>(round(100 * aborted / total)),
                   kMetricThermalAbortedFanTurnOnMax);
  ExpectEnumMetric(kMetricThermalMultipleFanTurnOnName,
                   static_cast<int>(round(100 * multiple / total)),
                   kMetricThermalMultipleFanTurnOnMax);
  daemon_.SendThermalMetrics(aborted, turned_on, multiple);
  // The next call should fail and not send a metric.
  // If it does, spurious SendEnumMetric calls will trigger a test failure
  daemon_.SendThermalMetrics(0, 0, multiple);
}

}  // namespace power_manager
