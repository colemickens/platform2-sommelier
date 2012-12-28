// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_INTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_INTERNAL_BACKLIGHT_CONTROLLER_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/monitor_reconfigure.h"
#include "power_manager/powerd/powerd.h"

typedef int gboolean;
typedef unsigned int guint;

namespace power_manager {

namespace system {
class BacklightInterface;
}  // namespace system

// Controls the internal backlight on devices with built-in displays.
//
// In the context of this class, "percent" refers to a double-precision
// brightness percentage in the range [0.0, 100.0] (where 0 indicates a
// fully-off backlight), while "level" refers to a 64-bit hardware-specific
// brightness in the range [0, max-brightness-per-sysfs].
class InternalBacklightController : public BacklightController {
 public:
  // Percent corresponding to |min_visible_level_|, which takes the role of the
  // lowest brightness step before the screen is turned off.  Exposed here for
  // tests.
  static const double kMinVisiblePercent;

  InternalBacklightController(system::BacklightInterface* backlight,
                              PowerPrefs* prefs,
                              AmbientLightSensor* sensor);
  virtual ~InternalBacklightController();

  int64 target_level_for_testing() const { return target_level_; }
  TransitionStyle last_transition_style_for_testing() const {
    return last_transition_style_;
  }

  // Converts between [0, 100] and [0, |max_level_|] brightness scales.
  double LevelToPercent(int64 level);
  int64 PercentToLevel(double percent);

  // BacklightController implementation:
  virtual bool Init() OVERRIDE;
  virtual void SetMonitorReconfigure(
      MonitorReconfigureInterface* monitor_reconfigure) OVERRIDE;
  virtual void SetObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual double GetTargetBrightnessPercent() OVERRIDE;
  virtual bool GetCurrentBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetCurrentBrightnessPercent(double percent,
                                           BrightnessChangeCause cause,
                                           TransitionStyle style) OVERRIDE;
  virtual bool IncreaseBrightness(BrightnessChangeCause cause) OVERRIDE;
  virtual bool DecreaseBrightness(bool allow_off,
                                  BrightnessChangeCause cause) OVERRIDE;
  virtual bool SetPowerState(PowerState state) OVERRIDE;
  virtual PowerState GetPowerState() const OVERRIDE;
  virtual bool OnPlugEvent(bool is_plugged) OVERRIDE;
  virtual bool IsBacklightActiveOff() OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // BacklightInterfaceObserver implementation:
  virtual void OnBacklightDeviceChanged();

  // Implementation of AmbientLightSensorObserver
  virtual void OnAmbientLightChanged(AmbientLightSensor* sensor) OVERRIDE;

 private:
  friend class DaemonTest;
  friend class InternalBacklightControllerTest;
  FRIEND_TEST(DaemonTest, GenerateEndOfSessionMetrics);
  FRIEND_TEST(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetric);
  FRIEND_TEST(DaemonTest,
              GenerateNumberOfAlsAdjustmentsPerSessionMetricOverflow);
  FRIEND_TEST(DaemonTest,
              GenerateNumberOfAlsAdjustmentsPerSessionMetricUnderflow);
  FRIEND_TEST(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetric);
  FRIEND_TEST(DaemonTest,
              GenerateUserBrightnessAdjustmentsPerSessionMetricOverflow);
  FRIEND_TEST(DaemonTest,
              GenerateUserBrightnessAdjustmentsPerSessionMetricUnderflow);

  // Clamp |percent| to fit between LevelToPercent(min_visible_level_) and 100.
  double ClampPercentToVisibleRange(double percent);

  void ReadPrefs();
  void WritePrefs();

  // Applies previously-configured brightness to the backlight and updates
  // |target_percent_|.  In the active and already-dimmed states, the new
  // brightness is the sum of |als_offset_percent_| and
  // |*current_offset_percent_|.
  //
  // Returns true if the brightness was set and false otherwise.
  // If |adjust_brightness_offset| is true, |*current_offset_percent_| is
  // updated (it can change due to clamping of the target brightness).
  bool WriteBrightness(bool adjust_brightness_offset,
                       BrightnessChangeCause cause,
                       TransitionStyle style);

  // Changes the brightness to |target_level|.  Use style = TRANSITION_FAST
  // to change brightness with smoothing effects.
  bool SetBrightness(int64 target_level, TransitionStyle style);

  // Changes |selection|'s state to |state| after |delay|.  If another change
  // has already been scheduled, it will be aborted.
  void SetScreenPowerState(
      ScreenPowerOutputSelection selection,
      ScreenPowerState state,
      base::TimeDelta delay);

  // Timeout callback that sets the passed-in state, resets
  // |set_screen_power_state_timeout_id_|, and returns FALSE to cancel the
  // timeout.
  SIGNAL_CALLBACK_PACKED_2(InternalBacklightController,
                           gboolean,
                           HandleSetScreenPowerStateTimeout,
                           ScreenPowerOutputSelection,
                           ScreenPowerState);

  // Backlight used for dimming. Non-owned.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Non-owned.
  PowerPrefs* prefs_;

  // Light sensor we need to register for updates from.  Non-owned.
  AmbientLightSensor* light_sensor_;

  // Use MonitorReconfigure to turn on/off the display.
  MonitorReconfigureInterface* monitor_reconfigure_;

  // Observer for changes to the brightness level.  Not owned by us.
  BacklightControllerObserver* observer_;

  // Indicate whether ALS value has been read before.
  bool has_seen_als_event_;

  // The brightness offset recommended by the ambient light sensor.  Never
  // negative.
  double als_offset_percent_;

  // Prevent small light sensor changes from updating the backlight.
  double als_hysteresis_percent_;

  // Also apply temporal hysteresis to light sensor responses.
  AlsHysteresisState als_temporal_state_;
  int als_temporal_count_;

  // Count of the number of adjustments that the ALS has caused
  int als_adjustment_count_;

  // Count of the number of adjustments that the user has caused
  int user_adjustment_count_;

  // User adjustable brightness offset when AC plugged.
  double plugged_offset_percent_;

  // User adjustable brightness offset when AC unplugged.
  double unplugged_offset_percent_;

  // Pointer to currently in-use user brightness offset: either
  // |plugged_offset_percent_|, |unplugged_offset_percent_|, or NULL.
  double* current_offset_percent_;

  // The offset when the backlight was last in the active state.  It is taken
  // from |*current_offset_percent| and does not include the ALS offset, which
  // can vary between suspend and resume.  This is used to restore the backlight
  // when returning to the active state.
  double last_active_offset_percent_;

  // Backlight power state, used to distinguish between various cases.
  // Backlight nonzero, backlight zero, backlight idle-dimmed, etc.
  PowerState state_;

  // Whether the computer is plugged in.
  PluggedState plugged_state_;

  // Target brightness in the range [0, 100].
  double target_percent_;

  // Maximum raw brightness level for |backlight_| (0 is assumed to be the
  // minimum, with the backlight turned off).
  int64 max_level_;

  // Minimum raw brightness level that we'll stop at before turning the
  // backlight off entirely when adjusting the brightness down.  Note that we
  // can still quickly animate through lower (still technically visible) levels
  // while transitioning to the off state; this is the minimum level that we'll
  // use in the steady state while the backlight is on.
  int64 min_visible_level_;

  // Indicates whether transitions between 0 and |min_visible_level_| must be
  // instant, i.e. the brightness may not smoothly transition between those
  // levels.
  bool instant_transitions_below_min_level_;

  // If true, we ignore readings from the ambient light sensor.  Controlled by
  // kDisableALSPref.
  bool ignore_ambient_light_;

  // Percentage by which we offset the brightness in response to increase and
  // decrease requests.
  double step_percent_;

  // Percentage, in the range [0.0, 100.0], to which we dim the backlight on
  // idle.
  double idle_brightness_percent_;

  // Brightness level fractions (e.g. 140/200) are raised to this power when
  // converting them to percents.  A value below 1.0 gives us more granularity
  // at the lower end of the range and less at the upper end.
  double level_to_percent_exponent_;

  // Flag is set if a backlight device exists.
  bool is_initialized_;

  // The destination hardware brightness used for brightness transitions.
  int64 target_level_;

  // Flag to indicate whether the state before suspended is idle off;
  bool suspended_through_idle_off_;

  // Source ID for timeout that will run HandleSetScreenPowerStateTimeout(), or
  // 0 if no timeout is currently set.
  guint set_screen_power_state_timeout_id_;

  // Screen off delay when user sets brightness to 0.
  base::TimeDelta turn_off_screen_timeout_;

  // Last transition style used by SetBrightness().  Useful for tests.
  TransitionStyle last_transition_style_;

  DISALLOW_COPY_AND_ASSIGN(InternalBacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_INTERNAL_BACKLIGHT_CONTROLLER_H_
