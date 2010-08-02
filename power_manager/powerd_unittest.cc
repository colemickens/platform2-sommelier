// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdint.h>
#include <X11/extensions/XTest.h>

#include "base/logging.h"
#include "metrics/metrics_library_mock.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/mock_video_detector.h"
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
static const int64 kSmallInterval = 250;
static const int64 kBigInterval = kSmallInterval * 4;
static const int64 kPluggedDim = kBigInterval;
static const int64 kPluggedOff = 2 * kBigInterval;
static const int64 kPluggedSuspend = 3 * kBigInterval;
static const int64 kUnpluggedDim = kPluggedDim;
static const int64 kUnpluggedOff = kPluggedOff;
static const int64 kUnpluggedSuspend = kPluggedSuspend;
static const int64 kLockMs = 60 * 10 * 1000;

bool CheckMetricInterval(time_t now, time_t last, time_t interval);

static gboolean FakeMotionEvent(gpointer data) {
  Display* display = static_cast<Display*>(data);
  XTestFakeRelativeMotionEvent(display, 0, 0, 0);
  XSync(display, false);
  return false;
}

class DaemonTest : public Test {
 public:
  DaemonTest()
      : prefs_(FilePath(".")),
        backlight_ctl_(&backlight_, &prefs_),
        daemon_(&backlight_ctl_, &prefs_, &metrics_lib_, &video_detector_) {}

  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor.
    EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
    EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
    EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    prefs_.WriteSetting("plugged_brightness_offset", kPluggedBrightness);
    prefs_.WriteSetting("unplugged_brightness_offset", kUnpluggedBrightness);
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

  // Adds a metrics library mock expectation for the battery discharge
  // rate metric with the given |sample|.
  void ExpectBatteryDischargeRateMetric(int sample) {
    ExpectMetric(Daemon::kMetricBatteryDischargeRateName, sample,
                 Daemon::kMetricBatteryDischargeRateMin,
                 Daemon::kMetricBatteryDischargeRateMax,
                 Daemon::kMetricBatteryDischargeRateBuckets);
  }

  // Adds a metrics library mock expectation for the remaining battery
  // charge metric with the given |sample|.
  void ExpectBatteryRemainingChargeMetric(int sample) {
    ExpectEnumMetric(Daemon::kMetricBatteryRemainingChargeName, sample,
                     Daemon::kMetricBatteryRemainingChargeMax);
  }

  // Adds a metrics library mock expectation for the battery's
  // remaining to empty metric with the given |sample|.
  void ExpectBatteryTimeToEmptyMetric(int sample) {
    ExpectMetric(Daemon::kMetricBatteryTimeToEmptyName, sample,
                 Daemon::kMetricBatteryTimeToEmptyMin,
                 Daemon::kMetricBatteryTimeToEmptyMax,
                 Daemon::kMetricBatteryTimeToEmptyBuckets);
  }

  // Resets all fields of |info| to 0.
  void ResetPowerStatus(chromeos::PowerStatus& status) {
    memset(&status, 0, sizeof(chromeos::PowerStatus));
  }

  StrictMock<MockBacklight> backlight_;
  StrictMock<MockVideoDetector> video_detector_;
  PowerPrefs prefs_;
  chromeos::PowerStatus status_;
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
      status_, Daemon::kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(Daemon::kMetricBatteryDischargeRateInterval,
            daemon_.battery_discharge_rate_metric_last_);

  status_.battery_energy_rate = 4.5;
  ExpectBatteryDischargeRateMetric(4500);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, Daemon::kMetricBatteryDischargeRateInterval - 1));
  EXPECT_EQ(Daemon::kMetricBatteryDischargeRateInterval - 1,
            daemon_.battery_discharge_rate_metric_last_);

  status_.battery_energy_rate = 6.4;
  ExpectBatteryDischargeRateMetric(6400);
  EXPECT_TRUE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, 2 * Daemon::kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(2 * Daemon::kMetricBatteryDischargeRateInterval,
            daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricInterval) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(status_,
                                                          /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, Daemon::kMetricBatteryDischargeRateInterval - 1));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected) {
  EXPECT_EQ(kPowerUnknown, daemon_.plugged_state_);

  status_.battery_energy_rate = 4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, Daemon::kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, 2 * Daemon::kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive) {
  daemon_.plugged_state_ = kPowerDisconnected;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, Daemon::kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);

  status_.battery_energy_rate = -4.0;
  EXPECT_FALSE(daemon_.GenerateBatteryDischargeRateMetric(
      status_, 2 * Daemon::kMetricBatteryDischargeRateInterval));
  EXPECT_EQ(0, daemon_.battery_discharge_rate_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingChargeMetric) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_percentage = 10.4;
  ExpectBatteryRemainingChargeMetric(10);
  EXPECT_TRUE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, Daemon::kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(Daemon::kMetricBatteryRemainingChargeInterval,
            daemon_.battery_remaining_charge_metric_last_);

  status_.battery_percentage = 11.6;
  ExpectBatteryRemainingChargeMetric(12);
  EXPECT_TRUE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, Daemon::kMetricBatteryRemainingChargeInterval - 1));
  EXPECT_EQ(Daemon::kMetricBatteryRemainingChargeInterval - 1,
            daemon_.battery_remaining_charge_metric_last_);

  status_.battery_percentage = 14.5;
  ExpectBatteryRemainingChargeMetric(15);
  EXPECT_TRUE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, 2 * Daemon::kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(2 * Daemon::kMetricBatteryRemainingChargeInterval,
            daemon_.battery_remaining_charge_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingChargeMetricInterval) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_percentage = 13.0;
  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(status_,
                                                            /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);

  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, Daemon::kMetricBatteryRemainingChargeInterval - 1));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryRemainingChargeMetricNotDisconnected) {
  EXPECT_EQ(kPowerUnknown, daemon_.plugged_state_);

  status_.battery_percentage = 20.0;
  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, Daemon::kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);

  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryRemainingChargeMetric(
      status_, 2 * Daemon::kMetricBatteryRemainingChargeInterval));
  EXPECT_EQ(0, daemon_.battery_remaining_charge_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryTimeToEmptyMetric) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_time_to_empty = 90;
  ExpectBatteryTimeToEmptyMetric(2);
  EXPECT_TRUE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, Daemon::kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(Daemon::kMetricBatteryTimeToEmptyInterval,
            daemon_.battery_time_to_empty_metric_last_);

  status_.battery_time_to_empty = 89;
  ExpectBatteryTimeToEmptyMetric(1);
  EXPECT_TRUE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, Daemon::kMetricBatteryTimeToEmptyInterval - 1));
  EXPECT_EQ(Daemon::kMetricBatteryTimeToEmptyInterval - 1,
            daemon_.battery_time_to_empty_metric_last_);

  status_.battery_time_to_empty = 151;
  ExpectBatteryTimeToEmptyMetric(3);
  EXPECT_TRUE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, 2 * Daemon::kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(2 * Daemon::kMetricBatteryTimeToEmptyInterval,
            daemon_.battery_time_to_empty_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryTimeToEmptyMetricInterval) {
  daemon_.plugged_state_ = kPowerDisconnected;
  status_.battery_time_to_empty = 100;
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(status_, /* now */ 0));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, Daemon::kMetricBatteryTimeToEmptyInterval - 1));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
}

TEST_F(DaemonTest, GenerateBatteryTimeToEmptyMetricNotDisconnected) {
  EXPECT_EQ(kPowerUnknown, daemon_.plugged_state_);

  status_.battery_time_to_empty = 120;
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, Daemon::kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);

  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_FALSE(daemon_.GenerateBatteryTimeToEmptyMetric(
      status_, 2 * Daemon::kMetricBatteryTimeToEmptyInterval));
  EXPECT_EQ(0, daemon_.battery_time_to_empty_metric_last_);
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

static gboolean QuitLoop(gpointer data) {
  GMainLoop* loop = static_cast<GMainLoop*>(data);
  g_main_loop_quit(loop);
  return false;
}

static gboolean SetPlugged(gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  daemon->SetPlugged(true);
  return false;
}

static gboolean SetUnplugged(gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  daemon->SetPlugged(false);
  return false;
}

TEST_F(DaemonTest, GenerateMetricsOnIdleEvent) {
  {
    InSequence metrics;
    EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
        .WillOnce(Return(true));
    // Mouse event 1 here. Idle -> Active transition
    // Check Video state.
    EXPECT_CALL(video_detector_, GetVideoActivity(NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(false),
                        Return(true)));
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kUnpluggedBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(0))
        .WillOnce(Return(true));
    EXPECT_CALL(metrics_lib_,
        SendToUMA("Power.IdleTimeOnBattery", _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(metrics_lib_,
        SendToUMA("Power.IdleTimeAfterDimOnBattery", _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(0),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
        .WillOnce(Return(true));
    // Mouse event 2 here. Idle -> Active transition
    // Check Video state.
    EXPECT_CALL(video_detector_, GetVideoActivity(NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(false),
                        Return(true)));
   EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kUnpluggedBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(0))
        .WillOnce(Return(true));
    EXPECT_CALL(metrics_lib_,
        SendToUMA("Power.IdleTimeOnBattery", _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(metrics_lib_,
        SendToUMA("Power.IdleTimeAfterScreenOffOnBattery", _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(0),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
        .WillOnce(Return(true));
    EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
        .WillOnce(Return(true));
    EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
        .WillOnce(Return(true));
    // Mouse event 3 here. Idle -> Active transition
    // Check Video state.
    EXPECT_CALL(video_detector_, GetVideoActivity(NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(false),
                        Return(true)));
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kPluggedBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(0))
        .WillOnce(Return(true));
    EXPECT_CALL(metrics_lib_,
        SendToUMA("Power.IdleTimeOnBattery", _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(metrics_lib_,
        SendToUMA("Power.IdleTimeAfterDimOnBattery", _, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(0),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
        .WillOnce(Return(true));
    // Mouse event 4 here. Idle -> Active transition
    // Check Video state.
    EXPECT_CALL(video_detector_, GetVideoActivity(NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(false),
                        Return(true)));
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kPluggedBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    EXPECT_CALL(backlight_, SetBrightness(0))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(gdk_init_check(NULL, NULL));
  FakeMotionEvent(GDK_DISPLAY());
  daemon_.plugged_dim_ms_ = kPluggedDim;
  daemon_.plugged_off_ms_ = kPluggedOff;
  daemon_.plugged_suspend_ms_ = INT64_MAX;
  daemon_.unplugged_dim_ms_ = kUnpluggedDim;
  daemon_.unplugged_off_ms_ = kUnpluggedOff;
  daemon_.unplugged_suspend_ms_ = INT64_MAX;
  daemon_.default_lock_ms_ = kLockMs;
  daemon_.idle_.Init(&daemon_);
  daemon_.SetPlugged(false);
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_timeout_add(kBigInterval + kSmallInterval, FakeMotionEvent, GDK_DISPLAY());
  g_timeout_add(4 * kBigInterval, FakeMotionEvent, GDK_DISPLAY());
  g_timeout_add(4 * kBigInterval + 2 * kSmallInterval, SetPlugged, &daemon_);
  g_timeout_add(5 * kBigInterval + kSmallInterval, FakeMotionEvent,
                GDK_DISPLAY());
  g_timeout_add(5 * kBigInterval + 2 * kSmallInterval, SetUnplugged, &daemon_);
  g_timeout_add(6 * kBigInterval + 3 * kSmallInterval, FakeMotionEvent,
                GDK_DISPLAY());
  g_timeout_add(9 * kBigInterval, QuitLoop, loop);
  g_main_loop_run(loop);
}

TEST_F(DaemonTest, GenerateBacklightLevelMetric) {
  daemon_.idle_state_ = Daemon::kIdleDim;
  daemon_.GenerateBacklightLevelMetric(&daemon_);
  daemon_.idle_state_ = Daemon::kIdleNormal;
  daemon_.plugged_state_ = kPowerDisconnected;
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  ExpectEnumMetric("Power.BacklightLevelOnBattery",
                   kDefaultBrightness, kMaxBrightness);
  daemon_.GenerateBacklightLevelMetric(&daemon_);
  daemon_.plugged_state_ = kPowerConnected;
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  ExpectEnumMetric("Power.BacklightLevelOnAC",
                   kDefaultBrightness, kMaxBrightness);
  daemon_.GenerateBacklightLevelMetric(&daemon_);
}

}  // namespace power_manager
