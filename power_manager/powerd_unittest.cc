// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/powerd.h"

namespace power_manager {

using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::Test;

static const int64 kDefaultBrightness = 50;
static const int64 kMaxBrightness = 100;
static const int64 kPluggedBrightness = 70;
static const int64 kUnpluggedBrightness = 30;
static const int64 kPluggedDim = 60 * 1000;
static const int64 kPluggedOff = 120 * 1000;
static const int64 kPluggedSuspend = 10 * 60 * 1000;
static const int64 kUnpluggedDim = 60 * 1000;
static const int64 kUnpluggedOff = 120 * 1000;
static const int64 kUnpluggedSuspend = 10 * 60 * 1000;

class DaemonTest : public Test {
 public:
  DaemonTest()
      : prefs_(FilePath(".")),
        backlight_ctl_(&backlight_, &prefs_),
        daemon_(&backlight_ctl_, &prefs_, &metrics_lib_) {}

  virtual void SetUp() {
    ASSERT_TRUE(gdk_init_check(NULL, NULL));

    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    prefs_.WriteSetting("plugged_brightness_offset", kPluggedBrightness);
    prefs_.WriteSetting("unplugged_brightness_offset", kUnpluggedBrightness);
    prefs_.WriteSetting("plugged_dim_ms", kPluggedDim);
    prefs_.WriteSetting("plugged_off_ms", kPluggedOff);
    prefs_.WriteSetting("plugged_suspend_ms", kPluggedSuspend);
    prefs_.WriteSetting("unplugged_dim_ms", kUnpluggedDim);
    prefs_.WriteSetting("unplugged_off_ms", kUnpluggedOff);
    prefs_.WriteSetting("unplugged_suspend_ms", kUnpluggedSuspend);
    CHECK(backlight_ctl_.Init());
    daemon_.Init();
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

  // Adds a metrics library mock expectation for the battery discharge
  // rate metric with the given |sample|.
  void ExpectBatteryDischargeRateMetric(int sample) {
    ExpectMetric(Daemon::kMetricBatteryDischargeRateName, sample,
                 Daemon::kMetricBatteryDischargeRateMin,
                 Daemon::kMetricBatteryDischargeRateMax,
                 Daemon::kMetricBatteryDischargeRateBuckets);
  }

  // Resets all fields of |info| to 0.
  void ResetPowerStatus(chromeos::PowerStatus& info) {
    memset(&info, 0, sizeof(chromeos::PowerStatus));
  }

  MockBacklight backlight_;
  PowerPrefs prefs_;
  BacklightController backlight_ctl_;

  // StrictMock turns all unexpected calls into hard failures.
  StrictMock<MetricsLibraryMock> metrics_lib_;
  Daemon daemon_;
};

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetric) {
  chromeos::PowerStatus info;
  ResetPowerStatus(info);
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = Daemon::kPowerDisconnected;
  info.battery_energy_rate = 5.0;
  ExpectBatteryDischargeRateMetric(5000);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 30));
  EXPECT_EQ(30, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricBackInTime) {
  chromeos::PowerStatus info;
  ResetPowerStatus(info);

  daemon_.plugged_state_ = Daemon::kPowerDisconnected;
  daemon_.battery_discharge_rate_metric_last_ = 30;
  info.battery_energy_rate = 4.5;
  ExpectBatteryDischargeRateMetric(4500);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 29));
  EXPECT_EQ(29, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricInterval) {
  chromeos::PowerStatus info;
  ResetPowerStatus(info);
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = Daemon::kPowerDisconnected;
  info.battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 29));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected) {
  chromeos::PowerStatus info;
  ResetPowerStatus(info);
  EXPECT_EQ(Daemon::kPowerUnknown, daemon_.plugged_state_);
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 30));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = Daemon::kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 60));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive) {
  chromeos::PowerStatus info;
  ResetPowerStatus(info);
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = Daemon::kPowerDisconnected;
  info.battery_energy_rate = 0.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 30));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  info.battery_energy_rate = -4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(info, /* now */ 60));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateMetricsOnPowerEvent) {
  chromeos::PowerStatus info;
  ResetPowerStatus(info);
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = Daemon::kPowerDisconnected;
  info.battery_energy_rate = 4.9;
  ExpectBatteryDischargeRateMetric(4900);
  daemon_.GenerateMetricsOnPowerEvent(info);
  EXPECT_LT(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, SendMetric) {
  ExpectMetric("Dummy.Metric", 3, 1, 100, 50);
  EXPECT_TRUE(daemon_.SendMetric("Dummy.Metric", /* sample */ 3,
                                 /* min */ 1, /* max */ 100, /* buckets */ 50));
}

}  // namespace power_manager
