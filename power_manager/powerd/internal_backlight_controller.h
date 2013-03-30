// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_INTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_INTERNAL_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/powerd.h"
#include "power_manager/powerd/system/ambient_light_observer.h"

namespace power_manager {

namespace system {
class AmbientLightSensorInterface;
class BacklightInterface;
class DisplayPowerSetterInterface;
}  // namespace system

// Controls the internal backlight on devices with built-in displays.
//
// In the context of this class, "percent" refers to a double-precision
// brightness percentage in the range [0.0, 100.0] (where 0 indicates a
// fully-off backlight), while "level" refers to a 64-bit hardware-specific
// brightness in the range [0, max-brightness-per-sysfs].
class InternalBacklightController : public BacklightController,
                                    public system::AmbientLightObserver {
 public:
  // Maximum number of brightness adjustment steps.
  static const int64 kMaxBrightnessSteps;

  // Percent corresponding to |min_visible_level_|, which takes the role of the
  // lowest brightness step before the screen is turned off.
  static const double kMinVisiblePercent;

  InternalBacklightController(
      system::BacklightInterface* backlight,
      PrefsInterface* prefs,
      system::AmbientLightSensorInterface* sensor,
      system::DisplayPowerSetterInterface* display_power_setter);
  virtual ~InternalBacklightController();

  // Initializes the object.
  bool Init();

  // Converts between [0, 100] and [0, |max_level_|] brightness scales.
  double LevelToPercent(int64 level);
  int64 PercentToLevel(double percent);

  // BacklightController implementation:
  virtual void AddObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE;
  virtual void HandleDisplayModeChange(DisplayMode mode) OVERRIDE;
  virtual void HandleSessionStateChange(SessionState state) OVERRIDE;
  virtual void HandlePowerButtonPress() OVERRIDE;
  virtual void HandleUserActivity() OVERRIDE;
  virtual void SetDimmedForInactivity(bool dimmed) OVERRIDE;
  virtual void SetOffForInactivity(bool off) OVERRIDE;
  virtual void SetSuspended(bool suspended) OVERRIDE;
  virtual void SetShuttingDown(bool shutting_down) OVERRIDE;
  virtual void SetDocked(bool docked) OVERRIDE;
  virtual bool GetBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetUserBrightnessPercent(double percent, TransitionStyle style)
      OVERRIDE;
  virtual bool IncreaseUserBrightness() OVERRIDE;
  virtual bool DecreaseUserBrightness(bool allow_off) OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // system::AmbientLightObserver implementation:
  virtual void OnAmbientLightChanged(
      system::AmbientLightSensorInterface* sensor) OVERRIDE;

 private:
  // TODO(derat): Move this and the related code into a separate class,
  // shared between InternalBacklightController and
  // KeyboardBacklightController, that handles ambient light hysteresis.
  enum AlsHysteresisState {
    ALS_HYST_IDLE,
    ALS_HYST_DOWN,
    ALS_HYST_UP,
    ALS_HYST_IMMEDIATE,
  };

  // Returns the brightness percent that should be used when the system is
  // in an undimmed state (which is typically just the appropriate
  // user-set offset plus the current ambient-light-contributed offset).
  double CalculateUndimmedBrightnessPercent() const;

  // Increases the user-set brightness to the minimum visible level if it's
  // currently set to zero.  Note that the brightness is left unchanged if
  // an external display is connected to avoid resizing the desktop.
  void EnsureUserBrightnessIsNonzero();

  // Updates the current brightness after assessing the current state
  // (based on |power_source_|, |dimmed_for_inactivity_|, etc.).  Should be
  // called whenever the state changes.
  void UpdateState();

  // Stores the brightness percent that should be used when the display is
  // in the undimmed state.  If the display is currently in the undimmed
  // state, additionally calls ApplyBrightnessPercent() to update the
  // backlight brightness.  Returns true if the brightness was changed.
  bool SetUndimmedBrightnessPercent(double percent,
                                    TransitionStyle style,
                                    BrightnessChangeCause);

  // Sets |backlight_|'s brightness to |percent| over |transition|.  If the
  // brightness changed, notifies |observers_| that the change was due to
  // |cause| and returns true.
  bool ApplyBrightnessPercent(double percent,
                              TransitionStyle transition,
                              BrightnessChangeCause cause);

  // Configures |backlight_| to resume from suspend at |resume_percent|.
  bool ApplyResumeBrightnessPercent(double resume_percent);

  void ReadPrefs();
  void WritePrefs();

  // Updates displays to |state| after |delay| if |state| doesn't match
  // |display_power_state_|.  If another change has already been scheduled,
  // it will be aborted.
  void SetDisplayPower(chromeos::DisplayPowerState state,
                       base::TimeDelta delay);

  // Backlight used for dimming. Non-owned.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Non-owned.
  PrefsInterface* prefs_;

  // Light sensor we need to register for updates from.  Non-owned.
  system::AmbientLightSensorInterface* light_sensor_;

  // Used to turn displays on and off.
  system::DisplayPowerSetterInterface* display_power_setter_;

  // Observers for changes to the brightness level.
  ObserverList<BacklightControllerObserver> observers_;

  // Information describing the current state of the system.
  PowerSource power_source_;
  DisplayMode display_mode_;
  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool suspended_;
  bool shutting_down_;
  bool docked_;

  // Indicates whether OnAmbientLightChanged() and
  // HandlePowerSourceChange() have been called yet.
  bool has_seen_als_event_;
  bool has_seen_power_source_change_;

  // The brightness offset recommended by the ambient light sensor.  Never
  // negative.
  double als_offset_percent_;

  // Prevent small light sensor changes from updating the backlight.
  double als_hysteresis_percent_;

  // Also apply temporal hysteresis to light sensor responses.
  AlsHysteresisState als_temporal_state_;
  int als_temporal_count_;

  // Number of ambient-light- and user-triggered brightness adjustments.
  int als_adjustment_count_;
  int user_adjustment_count_;

  // User-adjustable brightness offset when AC is plugged or unplugged.
  // Possibly negative.
  double plugged_offset_percent_;
  double unplugged_offset_percent_;

  // True if the user explicitly requested zero brightness for the undimmed
  // state.
  bool user_requested_zero_;

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
  double dimmed_brightness_percent_;

  // Brightness level fractions (e.g. 140/200) are raised to this power when
  // converting them to percents.  A value below 1.0 gives us more granularity
  // at the lower end of the range and less at the upper end.
  double level_to_percent_exponent_;

  // |backlight_|'s current brightness level (or the level to which it's
  // transitioning).
  int64 current_level_;

  // Most-recently-requested display power state.
  chromeos::DisplayPowerState display_power_state_;

  // Screen off delay when user sets brightness to 0.
  base::TimeDelta turn_off_screen_timeout_;

  DISALLOW_COPY_AND_ASSIGN(InternalBacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_INTERNAL_BACKLIGHT_CONTROLLER_H_
