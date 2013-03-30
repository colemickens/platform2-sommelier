// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <cmath>

#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/mock_metrics_store.h"
#include "power_manager/powerd/mock_rolling_average.h"
#include "power_manager/powerd/powerd.h"

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
static const int kNumOfSessionsPerCharge = 100;
static const int64 kBatteryTime = 23;
static const int64 kBatteryTimeTooLarge = 72 * 60 * 60;  // 3 days of seconds
static const int64 kThresholdTime = 7;
static const int64 kAdjustedBatteryTime = kBatteryTime - kThresholdTime;
static const unsigned int kSampleWindowMax = 10;
static const unsigned int kSampleWindowMin = 1;
static const unsigned int kSampleWindowDiff = kSampleWindowMax
                                              - kSampleWindowMin;
static const unsigned int kSampleWindowMid = kSampleWindowMin
                                             + kSampleWindowDiff / 2;
const unsigned int kTaperTimeMax = 60*60;
const unsigned int kTaperTimeMin = 10*60;
const unsigned int kTaperTimeDiff = kTaperTimeMax - kTaperTimeMin;
static const int64 kTaperTimeMid = kTaperTimeMin + kTaperTimeDiff/2;
static const double kAdjustedBatteryPercentage = 98.0;
static const double kTestPercentageThreshold = 2.0;
static const int64 kTestTimeThreshold = 180;

// BacklightController implementation that returns dummy values.
class BacklightControllerStub : public BacklightController {
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

  // BacklightController implementation:
  virtual void AddObserver(BacklightControllerObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(BacklightControllerObserver* observer) OVERRIDE {}
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

bool CheckMetricInterval(time_t now, time_t last, time_t interval);

class DaemonTest : public Test {
 public:
  DaemonTest()
      : daemon_(&backlight_ctl_, &prefs_, &metrics_lib_, NULL,
                base::FilePath(".")),
        status_(&daemon_.power_status_) {}

  virtual void SetUp() {
    daemon_.set_disable_dbus_for_testing(true);
    // Tests initialization done by the daemon's constructor.
    EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

    prefs_.SetDouble(kPluggedBrightnessOffsetPref, 70);
    prefs_.SetDouble(kUnpluggedBrightnessOffsetPref, 30);
    ResetPowerStatus(status_);
    // Setting up the window taper values, since they are needed in some of
    // the tests.
    daemon_.sample_window_max_ = kSampleWindowMax;
    daemon_.sample_window_min_ = kSampleWindowMin;
    daemon_.sample_window_diff_ = kSampleWindowDiff;
    daemon_.taper_time_max_s_ = kTaperTimeMax;
    daemon_.taper_time_min_s_ = kTaperTimeMin;
    daemon_.taper_time_diff_s_ = kTaperTimeDiff;
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
    if (daemon_.plugged_state_ == PLUGGED_STATE_DISCONNECTED) {
      name_with_power_state += "OnBattery";
    } else if (daemon_.plugged_state_ == PLUGGED_STATE_CONNECTED) {
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
    if (daemon_.plugged_state_ == PLUGGED_STATE_DISCONNECTED) {
      name_with_power_state += "OnBattery";
    } else if (daemon_.plugged_state_ == PLUGGED_STATE_CONNECTED) {
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

  StrictMock<MockMetricsStore> metrics_store_;
  PluggedState plugged_state_;
  FakePrefs prefs_;
  BacklightControllerStub backlight_ctl_;

  StrictMock<MockRollingAverage> empty_average_;
  StrictMock<MockRollingAverage> full_average_;

  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;
  Daemon daemon_;
  PowerStatus* status_;
};

TEST_F(DaemonTest, AdjustWindowSizeMax) {
  empty_average_.ExpectChangeWindowSize(kSampleWindowMax);

  daemon_.AdjustWindowSize(kTaperTimeMax, &empty_average_, &full_average_);
}

TEST_F(DaemonTest, AdjustWindowSizeMin) {
  empty_average_.ExpectChangeWindowSize(kSampleWindowMin);
  daemon_.AdjustWindowSize(kTaperTimeMin, &empty_average_, &full_average_);
}

TEST_F(DaemonTest, AdjustWindowSizeCalc) {
  empty_average_.ExpectChangeWindowSize(kSampleWindowMid);
  daemon_.AdjustWindowSize(
      kTaperTimeMid, &empty_average_, &full_average_);
}

TEST_F(DaemonTest, CheckMetricInterval) {
  EXPECT_FALSE(CheckMetricInterval(29, 0, 30));
  EXPECT_TRUE(CheckMetricInterval(30, 0, 30));
  EXPECT_TRUE(CheckMetricInterval(29, 30, 100));
  EXPECT_FALSE(CheckMetricInterval(39, 30, 10));
  EXPECT_TRUE(CheckMetricInterval(40, 30, 10));
  EXPECT_TRUE(CheckMetricInterval(41, 30, 10));
}

TEST_F(DaemonTest, GenerateBacklightLevelMetric) {
  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  daemon_.screen_dim_timestamp_ = base::TimeTicks::Now();
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);

  daemon_.screen_dim_timestamp_ = base::TimeTicks();
  const int64 kCurrentPercent = 57;
  backlight_ctl_.set_percent(kCurrentPercent);
  ExpectEnumMetric("Power.BacklightLevelOnBattery", kCurrentPercent,
                   kMetricBacklightLevelMax);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);

  daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
  ExpectEnumMetric("Power.BacklightLevelOnAC", kCurrentPercent,
                   kMetricBacklightLevelMax);
  daemon_.GenerateBacklightLevelMetricThunk(&daemon_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetric) {
  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  status_->battery_energy_rate = 5.0;
  ExpectBatteryDischargeRateMetric(5000);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(kMetricBatteryDischargeRateInterval,
            daemon_.battery_discharge_rate_metric_last_);

  status_->battery_energy_rate = 4.5;
  ExpectBatteryDischargeRateMetric(4500);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, kMetricBatteryDischargeRateInterval - 1));
  EXPECT_EQ(kMetricBatteryDischargeRateInterval - 1,
            daemon_.battery_discharge_rate_metric_last_);

  status_->battery_energy_rate = 6.4;
  ExpectBatteryDischargeRateMetric(6400);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, 2 * kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(2 * kMetricBatteryDischargeRateInterval,
            daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricInterval) {
  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  status_->battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(*status_,
                                                          /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, kMetricBatteryDischargeRateInterval - 1));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected) {
  EXPECT_EQ(PLUGGED_STATE_UNKNOWN, daemon_.plugged_state_);

  status_->battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, 2 * kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive) {
  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  status_->battery_energy_rate = -4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      *status_, 2 * kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryInfoWhenChargeStartsMetric) {
  const double battery_percentages[] = { 10.1, 10.7,
                                         20.4, 21.6,
                                         60.4, 61.6,
                                         82.4, 82.5,
                                         102.4, 111.6};
  size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  status_->battery_is_present = true;
  plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  daemon_.GenerateBatteryInfoWhenChargeStartsMetric(plugged_state_,
                                                    *status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  plugged_state_ = PLUGGED_STATE_UNKNOWN;
  daemon_.GenerateBatteryInfoWhenChargeStartsMetric(plugged_state_,
                                                    *status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  status_->battery_is_present = false;
  plugged_state_ = PLUGGED_STATE_CONNECTED;
  daemon_.GenerateBatteryInfoWhenChargeStartsMetric(plugged_state_,
                                                    *status_);
  Mock::VerifyAndClearExpectations(&metrics_lib_);

  status_->battery_is_present = true;
  status_->battery_charge_full_design = 100;
  for (size_t i = 0; i < num_percentages; i++) {
    status_->battery_percentage = battery_percentages[i];
    status_->battery_charge_full = battery_percentages[i];
    int expected_percentage = round(status_->battery_percentage);

    ExpectBatteryInfoWhenChargeStartsMetric(expected_percentage);
    daemon_.GenerateBatteryInfoWhenChargeStartsMetric(plugged_state_,
                                                      *status_);
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetric) {
  static const uint adjustment_counts[] = {0, 100, 500, 1000};
  size_t num_counts = ARRAYSIZE_UNSAFE(adjustment_counts);

  for (size_t i = 0; i < num_counts; i++) {
    backlight_ctl_.set_num_als_adjustments(adjustment_counts[i]);
    ExpectNumberOfAlsAdjustmentsPerSessionMetric(adjustment_counts[i]);
    EXPECT_TRUE(
        daemon_.GenerateNumberOfAlsAdjustmentsPerSessionMetric(backlight_ctl_));
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetricOverflow) {
  backlight_ctl_.set_num_als_adjustments(
      kMetricNumberOfAlsAdjustmentsPerSessionMax + kAdjustmentsOffset);
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(
      kMetricNumberOfAlsAdjustmentsPerSessionMax);
  EXPECT_TRUE(
      daemon_.GenerateNumberOfAlsAdjustmentsPerSessionMetric(backlight_ctl_));
}

TEST_F(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetricUnderflow) {
  backlight_ctl_.set_num_als_adjustments(-kAdjustmentsOffset);
  EXPECT_FALSE(
      daemon_.GenerateNumberOfAlsAdjustmentsPerSessionMetric(backlight_ctl_));
}

TEST_F(DaemonTest, GenerateLengthOfSessionMetric) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now - base::TimeDelta::FromSeconds(kSessionLength);

  ExpectLengthOfSessionMetric(kSessionLength);
  EXPECT_TRUE(daemon_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(DaemonTest, GenerateLengthOfSessionMetricOverflow) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now - base::TimeDelta::FromSeconds(
      kMetricLengthOfSessionMax + kSessionLength);

  ExpectLengthOfSessionMetric(kMetricLengthOfSessionMax);
  EXPECT_TRUE(daemon_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(DaemonTest, GenerateLengthOfSessionMetricUnderflow) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now + base::TimeDelta::FromSeconds(kSessionLength);

  EXPECT_FALSE(daemon_.GenerateLengthOfSessionMetric(now, start));
}

TEST_F(DaemonTest, GenerateNumOfSessionsPerChargeMetric) {
  metrics_store_.ExpectIsInitialized(true);
  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(0);
  EXPECT_TRUE(daemon_.GenerateNumOfSessionsPerChargeMetric(&metrics_store_));
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectIsInitialized(true);
  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(
      kNumOfSessionsPerCharge);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(kNumOfSessionsPerCharge);
  EXPECT_TRUE(daemon_.GenerateNumOfSessionsPerChargeMetric(&metrics_store_));
  Mock::VerifyAndClearExpectations(&metrics_lib_);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  EXPECT_FALSE(daemon_.GenerateNumOfSessionsPerChargeMetric(NULL));
}

TEST_F(DaemonTest, HandleNumOfSessionsPerChargeOnSetPlugged) {
  metrics_store_.ExpectIsInitialized(true);
  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(
      kNumOfSessionsPerCharge);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  ExpectNumOfSessionsPerChargeMetric(kNumOfSessionsPerCharge);
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   PLUGGED_STATE_CONNECTED);
  Mock::VerifyAndClearExpectations(&metrics_lib_);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(0);
  metrics_store_.ExpectIncrementNumOfSessionsPerChargeMetric();
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   PLUGGED_STATE_DISCONNECTED);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(1);
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   PLUGGED_STATE_DISCONNECTED);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(
      kNumOfSessionsPerCharge);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  metrics_store_.ExpectIncrementNumOfSessionsPerChargeMetric();
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   PLUGGED_STATE_DISCONNECTED);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  metrics_store_.ExpectGetNumOfSessionsPerChargeMetric(-1);
  metrics_store_.ExpectResetNumOfSessionsPerChargeMetric();
  metrics_store_.ExpectIncrementNumOfSessionsPerChargeMetric();
  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   PLUGGED_STATE_DISCONNECTED);
  Mock::VerifyAndClearExpectations(&metrics_store_);

  daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                                   PLUGGED_STATE_UNKNOWN);

  EXPECT_DEATH(daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(
      NULL,
      PLUGGED_STATE_CONNECTED),
               ".*");
  EXPECT_DEATH(daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(
      NULL,
      PLUGGED_STATE_DISCONNECTED),
               ".*");
  EXPECT_DEATH(daemon_.HandleNumOfSessionsPerChargeOnSetPlugged(
      NULL,
      PLUGGED_STATE_UNKNOWN),
               ".*");
}

TEST_F(DaemonTest, GenerateEndOfSessionMetrics) {
  status_->battery_percentage = 10.1;
  int expected_percentage = round(status_->battery_percentage);
  ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);

  backlight_ctl_.set_num_als_adjustments(kAdjustmentsOffset);
  ExpectNumberOfAlsAdjustmentsPerSessionMetric(kAdjustmentsOffset);

  const int kNumUserAdjustments = 10;
  backlight_ctl_.set_num_user_adjustments(kNumUserAdjustments);
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks start = now - base::TimeDelta::FromSeconds(kSessionLength);
  ExpectLengthOfSessionMetric(kSessionLength);

  daemon_.GenerateEndOfSessionMetrics(*status_, backlight_ctl_, now, start);
}

TEST_F(DaemonTest, GenerateBatteryRemainingAtEndOfSessionMetric) {
  const double battery_percentages[] = {10.1, 10.7,
                                        20.4, 21.6,
                                        60.4, 61.6,
                                        82.4, 82.5};
  const size_t num_percentages = ARRAYSIZE_UNSAFE(battery_percentages);

  for (size_t i = 0; i < num_percentages; i++) {
    status_->battery_percentage = battery_percentages[i];
    int expected_percentage = round(status_->battery_percentage);

    daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtEndOfSessionMetric(
        *status_));

    daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtEndOfSessionMetric(
        *status_));

    daemon_.plugged_state_ = PLUGGED_STATE_UNKNOWN;
    ExpectBatteryRemainingAtEndOfSessionMetric(expected_percentage);
    EXPECT_FALSE(daemon_.GenerateBatteryRemainingAtEndOfSessionMetric(
        *status_));
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
    status_->battery_percentage = battery_percentages[i];
    int expected_percentage = round(status_->battery_percentage);

    daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtStartOfSessionMetric(
        *status_));

    daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_TRUE(daemon_.GenerateBatteryRemainingAtStartOfSessionMetric(
        *status_));

    daemon_.plugged_state_ = PLUGGED_STATE_UNKNOWN;
    ExpectBatteryRemainingAtStartOfSessionMetric(expected_percentage);
    EXPECT_FALSE(daemon_.GenerateBatteryRemainingAtStartOfSessionMetric(
        *status_));
    Mock::VerifyAndClearExpectations(&metrics_lib_);
  }
}

TEST_F(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetric) {
  const int kNumUserAdjustments = 10;
  backlight_ctl_.set_num_user_adjustments(kNumUserAdjustments);

  daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));

  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));

  daemon_.plugged_state_ = PLUGGED_STATE_UNKNOWN;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(kNumUserAdjustments);
  EXPECT_FALSE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

TEST_F(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetricOverflow) {
  backlight_ctl_.set_num_user_adjustments(
      kMetricUserBrightnessAdjustmentsPerSessionMax + kAdjustmentsOffset);
  daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
  ExpectUserBrightnessAdjustmentsPerSessionMetric(
      kMetricUserBrightnessAdjustmentsPerSessionMax);
  EXPECT_TRUE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

TEST_F(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetricUnderflow) {
  backlight_ctl_.set_num_user_adjustments(-kAdjustmentsOffset);
  daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
  EXPECT_FALSE(daemon_.GenerateUserBrightnessAdjustmentsPerSessionMetric(
      backlight_ctl_));
}

TEST_F(DaemonTest, GenerateMetricsOnPowerEvent) {
  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  status_->battery_energy_rate = 4.9;
  status_->battery_percentage = 32.5;
  status_->battery_time_to_empty = 10 * 60;
  ExpectBatteryDischargeRateMetric(4900);
  daemon_.GenerateMetricsOnPowerEvent(*status_);
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
  daemon_.plugged_state_ = PLUGGED_STATE_DISCONNECTED;
  ExpectMetric("Dummy.MetricOnBattery", 3, 1, 100, 50);
  EXPECT_TRUE(daemon_.SendMetricWithPowerState("Dummy.Metric", /* sample */ 3,
      /* min */ 1, /* max */ 100, /* buckets */ 50));
  daemon_.plugged_state_ = PLUGGED_STATE_CONNECTED;
  ExpectMetric("Dummy.MetricOnAC", 3, 1, 100, 50);
  EXPECT_TRUE(daemon_.SendMetricWithPowerState("Dummy.Metric", /* sample */ 3,
      /* min */ 1, /* max */ 100, /* buckets */ 50));
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

// TODO(derat): Rewrite these tests to not use GMock; they currently test all
// kinds of unimportant things like whether GetAverage() was called or not.
TEST_F(DaemonTest, UpdateAveragedTimesChargingAndCalculating) {
  status_->line_power_on = true;
  status_->is_calculating_battery_time = true;

  empty_average_.ExpectClear();
  full_average_.ExpectGetAverage(kBatteryTime);
  empty_average_.ExpectGetAverage(0);
  ExpectGoodBatteryInfoSample();

  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average_, &full_average_));
  EXPECT_EQ(0, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesChargingAndNotCalculating) {
  status_->line_power_on = true;
  status_->is_calculating_battery_time = false;
  status_->battery_time_to_full = kBatteryTime;

  full_average_.ExpectAddSample(kBatteryTime, kBatteryTime);
  empty_average_.ExpectClear();
  empty_average_.ExpectChangeWindowSize(10);
  full_average_.ExpectGetAverage(kBatteryTime);
  empty_average_.ExpectGetAverage(0);
  ExpectGoodBatteryInfoSample();

  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average_, &full_average_));
  EXPECT_EQ(0, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesDischargingAndCalculating) {
  status_->line_power_on = false;
  status_->is_calculating_battery_time = true;

  full_average_.ExpectClear();
  full_average_.ExpectGetAverage(0);
  empty_average_.ExpectGetAverage(kBatteryTime);
  ExpectGoodBatteryInfoSample();

  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average_, &full_average_));
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_->averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesDischargingAndNotCalculating) {
  status_->line_power_on = false;
  status_->is_calculating_battery_time = false;
  status_->battery_time_to_empty = kBatteryTime;
  daemon_.low_battery_shutdown_time_s_ = kThresholdTime;
  daemon_.low_battery_shutdown_percent_ = 0.0;

  empty_average_.ExpectAddSample(kAdjustedBatteryTime, kAdjustedBatteryTime);
  full_average_.ExpectClear();
  empty_average_.ExpectChangeWindowSize(1);
  full_average_.ExpectGetAverage(0);
  empty_average_.ExpectGetAverage(kBatteryTime);
  ExpectGoodBatteryInfoSample();

  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average_, &full_average_));
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_->averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesWithSetThreshold) {
  status_->line_power_on = false;
  status_->is_calculating_battery_time = false;
  status_->battery_time_to_empty = kBatteryTime;
  daemon_.low_battery_shutdown_time_s_ = kThresholdTime;
  daemon_.low_battery_shutdown_percent_ = 0.0;

  empty_average_.ExpectAddSample(kAdjustedBatteryTime, kAdjustedBatteryTime);
  full_average_.ExpectClear();
  empty_average_.ExpectChangeWindowSize(1);
  full_average_.ExpectGetAverage(0);
  empty_average_.ExpectGetAverage(kBatteryTime);
  ExpectGoodBatteryInfoSample();

  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average_, &full_average_));
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_->averaged_battery_time_to_full);
}

TEST_F(DaemonTest, UpdateAveragedTimesWithBadStatus) {
  status_->line_power_on = false;
  status_->is_calculating_battery_time = false;
  status_->battery_time_to_empty = kBatteryTimeTooLarge;
  daemon_.low_battery_shutdown_time_s_ = kThresholdTime;
  daemon_.low_battery_shutdown_percent_ = 0.0;

  ExpectBadBatteryInfoSample();
  EXPECT_FALSE(daemon_.UpdateAveragedTimes(&empty_average_, &full_average_));
  EXPECT_EQ(0, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_->averaged_battery_time_to_full);
  EXPECT_TRUE(status_->is_calculating_battery_time);
}

TEST_F(DaemonTest, UpdateAveragedTimesWithBadIgnoredValues) {
  const unsigned int kWindowSize = 10;
  RollingAverage empty_average;
  empty_average.Init(kWindowSize);
  RollingAverage full_average;
  full_average.Init(kWindowSize);

  // Bogus time-to-full values should be ignored while the battery is draining.
  status_->battery_percentage = 100.0;
  status_->line_power_on = false;
  status_->is_calculating_battery_time = false;
  status_->battery_time_to_empty = kBatteryTime;
  status_->battery_time_to_full = kBatteryTimeTooLarge;
  ExpectGoodBatteryInfoSample();
  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average, &full_average));
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(0, status_->averaged_battery_time_to_full);

  // Similarly, bogus times-to-empty should be ignored while charging.
  status_->line_power_on = true;
  status_->battery_time_to_empty = kBatteryTimeTooLarge;
  status_->battery_time_to_full = kBatteryTime;
  ExpectGoodBatteryInfoSample();
  EXPECT_TRUE(daemon_.UpdateAveragedTimes(&empty_average, &full_average));
  EXPECT_EQ(0, status_->averaged_battery_time_to_empty);
  EXPECT_EQ(kBatteryTime, status_->averaged_battery_time_to_full);
}

TEST_F(DaemonTest, GetDisplayBatteryPercent) {
  daemon_.low_battery_shutdown_time_s_ = kTestTimeThreshold;
  daemon_.low_battery_shutdown_percent_ = 0.0;
  daemon_.power_status_.battery_percentage = kAdjustedBatteryPercentage;

  // Return the adjusted value when charging.
  daemon_.power_status_.battery_state = BATTERY_STATE_CHARGING;
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(kAdjustedBatteryPercentage, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_ADJUSTED, daemon_.battery_report_state_);

  // Return 0.0 when battery is empty.
  daemon_.power_status_.battery_percentage = 0.0;
  daemon_.power_status_.battery_state = BATTERY_STATE_EMPTY;
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(0.0, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_ADJUSTED, daemon_.battery_report_state_);

  daemon_.power_status_.battery_percentage = kAdjustedBatteryPercentage;
  // Return 100% when battery is charged.
  daemon_.power_status_.battery_state = BATTERY_STATE_FULLY_CHARGED;
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(100.0, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_FULL, daemon_.battery_report_state_);

  // Pin the percentage at 100% right after going to charging.
  daemon_.battery_report_pinned_start_ = base::TimeTicks();
  daemon_.power_status_.battery_state = BATTERY_STATE_DISCHARGING;
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(100.0, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_PINNED, daemon_.battery_report_state_);
  EXPECT_NE(0, (base::TimeTicks() -
                daemon_.battery_report_pinned_start_).InMilliseconds());

  // Percentage should still be pinned part way through the window.
  daemon_.battery_report_pinned_start_ = base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(kBatteryPercentPinMs / 2);
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(100.0, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_PINNED, daemon_.battery_report_state_);

  // Percentage should stay at 100 as we finish being pinned.
  daemon_.battery_report_tapered_start_ = base::TimeTicks();
  daemon_.battery_report_pinned_start_ = base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(2 * kBatteryPercentPinMs);
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(100.0, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_TAPERED, daemon_.battery_report_state_);
  EXPECT_NE(0, (base::TimeTicks() -
                daemon_.battery_report_tapered_start_).InMilliseconds());

  // While tapering value should be between 100 and calculated adjusted value.
  daemon_.battery_report_tapered_start_ = base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(kBatteryPercentTaperMs / 2);
  daemon_.UpdateBatteryReportState();
  double ret_val = daemon_.GetDisplayBatteryPercent();
  EXPECT_GE(100.0, ret_val);
  EXPECT_LE(kAdjustedBatteryPercentage, ret_val);
  EXPECT_EQ(BATTERY_REPORT_TAPERED, daemon_.battery_report_state_);

  // After tapering the adjusted values should be used.
  daemon_.battery_report_tapered_start_ = base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(2 * kBatteryPercentTaperMs);
  daemon_.UpdateBatteryReportState();
  EXPECT_EQ(kAdjustedBatteryPercentage, daemon_.GetDisplayBatteryPercent());
  EXPECT_EQ(BATTERY_REPORT_ADJUSTED, daemon_.battery_report_state_);
}

TEST_F(DaemonTest, GetUsableBatteryPercent) {
  // Out of bounds low.
  daemon_.power_status_.battery_percentage = -1.0;
  EXPECT_EQ(0.0, daemon_.GetUsableBatteryPercent());

  // Out of bound high.
  daemon_.power_status_.battery_percentage = 101.0;
  EXPECT_EQ(100.0, daemon_.GetUsableBatteryPercent());

  // Time based threshold being used.
  daemon_.low_battery_shutdown_time_s_ = kTestTimeThreshold;
  daemon_.low_battery_shutdown_percent_ = 0.0;
  daemon_.power_status_.battery_percentage = kAdjustedBatteryPercentage;
  EXPECT_EQ(kAdjustedBatteryPercentage, daemon_.GetUsableBatteryPercent());

  // Percentage based threshold being used.
  daemon_.low_battery_shutdown_time_s_ = 0;
  daemon_.low_battery_shutdown_percent_ = kTestPercentageThreshold;
  daemon_.power_status_.battery_percentage = 100.0;
  EXPECT_EQ(100.0, daemon_.GetUsableBatteryPercent());
  daemon_.power_status_.battery_percentage = kTestPercentageThreshold;
  EXPECT_EQ(0.0, daemon_.GetUsableBatteryPercent());
  daemon_.power_status_.battery_percentage =
      100.0 - (100.0 - kTestPercentageThreshold) / 2;
  EXPECT_EQ(50.0, daemon_.GetUsableBatteryPercent());
}

}  // namespace power_manager
