// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_BACKLIGHT_CONTROLLER_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "base/basictypes.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/signal_callback.h"

typedef int gboolean;

namespace power_manager {

class AmbientLightSensor;
class BacklightInterface;
class PowerPrefsInterface;

enum PowerState {
  // User is active.
  BACKLIGHT_ACTIVE,

  // Dimmed due to inactivity.
  BACKLIGHT_DIM,

  // Got a request to go to BACKLIGHT_DIM while already at a lower level.
  BACKLIGHT_ALREADY_DIMMED,

  // Turned backlight off due to inactivity.
  BACKLIGHT_IDLE_OFF,

  // Machine is suspended.
  BACKLIGHT_SUSPENDED,

  // State has not yet been set.
  BACKLIGHT_UNINITIALIZED
};

enum PluggedState {
  kPowerDisconnected,
  kPowerConnected,
  kPowerUnknown,
};

enum AlsHysteresisState {
  ALS_HYST_IDLE,
  ALS_HYST_DOWN,
  ALS_HYST_UP,
  ALS_HYST_IMMEDIATE,
};

// Possible causes of changes to the backlight brightness level.
enum BrightnessChangeCause {
  // The brightness was changed automatically (in response to e.g. an idle
  // transition or AC getting plugged or unplugged).
  BRIGHTNESS_CHANGE_AUTOMATED,

  // The user requested that the brightness be changed.
  BRIGHTNESS_CHANGE_USER_INITIATED,
};

// Interface for observing changes made by the backlight controller.
class BacklightControllerObserver {
 public:
  // Invoked when the brightness level is changed.  |brightness_percent| is the
  // current brightness in the range [0, 100].
  virtual void OnScreenBrightnessChanged(double brightness_percent,
                                         BrightnessChangeCause cause) {}

 protected:
  ~BacklightControllerObserver() {}
};

// Controls the backlight.
//
// In the context of this class, "percent" refers to a double-precision
// brightness percentage in the range [0.0, 100.0] (where 0 indicates a
// fully-off backlight), while "level" refers to a 64-bit hardware-specific
// brightness in the range [0, max-brightness-per-sysfs].
class BacklightController : public BacklightInterfaceObserver {
 public:
  BacklightController(BacklightInterface* backlight,
                      PowerPrefsInterface* prefs);
  virtual ~BacklightController();

  void set_light_sensor(AmbientLightSensor* als) { light_sensor_ = als; }
  void set_observer(BacklightControllerObserver* observer) {
    observer_ = observer;
  }

  double target_percent() const { return target_percent_; }
  PowerState state() const { return state_; }
  int als_adjustment_count() const { return als_adjustment_count_; }
  int user_adjustment_count() const { return user_adjustment_count_; }

  // Initialize the object.  Note that this method is also reinvoked when the
  // backlight device changes.
  bool Init();

  // Get the current brightness of the backlight in the range [0, 100].
  // We may be in the process of smoothly transitioning to a different level.
  bool GetCurrentBrightnessPercent(double* percent);

  // Increase the brightness level of the backlight by one step.
  // Returns true if the brightness was changed, false otherwise.
  bool IncreaseBrightness(BrightnessChangeCause cause);

  // Decrease the brightness level of the backlight by one step.
  // Returns true if the brightness was changed, false otherwise.
  //
  // If |allow_off| is false, the backlight will never be entirely turned off.
  // This should be used with on-screen controls to prevent their becoming
  // impossible for the user to see.
  bool DecreaseBrightness(bool allow_off, BrightnessChangeCause cause);

  // Turn the backlight on or off.  Returns true if the state was successfully
  // changed.
  bool SetPowerState(PowerState state);

  // Mark the computer as plugged or unplugged, and adjust the brightness
  // appropriately.  Returns true if the brightness was set and false
  // otherwise.
  bool OnPlugEvent(bool is_plugged);

  // Update the brightness offset that is applied based on the current amount of
  // ambient light.
  void SetAlsBrightnessOffsetPercent(double percent);

  // Returns whether the user has manually turned backlight down to zero.
  bool IsBacklightActiveOff();

  // Converts between [0, 100] and [0, |max_level_|] brightness scales.
  double LevelToPercent(int64 level);
  int64 PercentToLevel(double percent);

  // BacklightInterfaceObserver implementation:
  virtual void OnBacklightDeviceChanged();

 private:
  friend class DaemonTest;
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

  // How to transition between brightness levels.
  enum TransitionStyle {
    TRANSITION_GRADUAL,
    TRANSITION_INSTANT,
  };

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

  // Changes the brightness to |target_level|.  Use style = TRANSITION_GRADUAL
  // to change brightness with smoothing effects.
  bool SetBrightness(int64 target_level, TransitionStyle style);

  // Callback function to set backlight brightness through the backlight
  // interface.  This is used by SetBrightness to change the brightness
  // over a series of steps.
  //
  // Example:
  //   Current brightness = 40
  //   Want to set brightness to 60 over 5 steps, so the steps are:
  //      40 -> 44 -> 48 -> 52 -> 56 -> 60
  //   Thus, SetBrightnessHard(level, target_level) would be called five times
  //   with the args:
  //      SetBrightnessHard(44, 60);
  //      SetBrightnessHard(48, 60);
  //      SetBrightnessHard(52, 60);
  //      SetBrightnessHard(56, 60);
  //      SetBrightnessHard(60, 60);
  SIGNAL_CALLBACK_PACKED_2(BacklightController, gboolean, SetBrightnessHard,
                           int64, int64);

  // Backlight used for dimming. Non-owned.
  BacklightInterface* backlight_;

  // Interface for saving preferences. Non-owned.
  PowerPrefsInterface* prefs_;

  // Light sensor we need to enable/disable on power events.  Non-owned.
  AmbientLightSensor* light_sensor_;

  // Observer for changes to the brightness level.  Not owned by us.
  BacklightControllerObserver* observer_;

  // Indicate whether ALS value has been read before.
  bool has_seen_als_event_;

  // The brightness offset recommended by the ambient light sensor.  Never
  // negative.
  double als_offset_percent_;

  // Prevent small light sensor changes from updating the backlight.
  double als_hysteresis_percent_;

  // Also apply temporal hysteresis to light sensor samples.
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

  // Flag to indicate whether there is an active brightness transition going on.
  bool is_in_transition_;

  DISALLOW_COPY_AND_ASSIGN(BacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
