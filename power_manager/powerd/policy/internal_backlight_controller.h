// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_INTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_INTERNAL_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/powerd/policy/ambient_light_handler.h"
#include "power_manager/powerd/policy/backlight_controller.h"

namespace power_manager {

class Clock;
class PrefsInterface;

namespace system {
class AmbientLightSensorInterface;
class BacklightInterface;
class DisplayPowerSetterInterface;
}  // namespace system

namespace policy {

// Controls the internal backlight on devices with built-in displays.
//
// In the context of this class, "percent" refers to a double-precision
// brightness percentage in the range [0.0, 100.0] (where 0 indicates a
// fully-off backlight), while "level" refers to a 64-bit hardware-specific
// brightness in the range [0, max-brightness-per-sysfs].
class InternalBacklightController : public BacklightController,
                                    public AmbientLightHandler::Delegate {
 public:
  // Maximum number of brightness adjustment steps.
  static const int64 kMaxBrightnessSteps;

  // Percent corresponding to |min_visible_level_|, which takes the role of the
  // lowest brightness step before the screen is turned off.
  static const double kMinVisiblePercent;

  // If an ambient light reading hasn't been seen after this many seconds,
  // give up on waiting for the sensor to be initialized and just set
  // |use_ambient_light_| to false.
  static const int kAmbientLightSensorTimeoutSec;

  InternalBacklightController();
  virtual ~InternalBacklightController();

  Clock* clock() { return clock_.get(); }

  // Initializes the object. Ownership of the passed-in pointers remains with
  // the caller.
  void Init(system::BacklightInterface* backlight,
            PrefsInterface* prefs,
            system::AmbientLightSensorInterface* sensor,
            system::DisplayPowerSetterInterface* display_power_setter);

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
  virtual void HandleUserActivity(UserActivityType type) OVERRIDE;
  virtual void HandlePolicyChange(const PowerManagementPolicy& policy) OVERRIDE;
  virtual void HandleChromeStart() OVERRIDE;
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

  // AmbientLightHandler::Delegate implementation:
  virtual void SetBrightnessPercentForAmbientLight(
      double brightness_percent,
      AmbientLightHandler::BrightnessChangeCause cause) OVERRIDE;

 private:
  // Snaps |percent| to the nearest step, as defined by |step_percent_|.
  double SnapBrightnessPercentToNearestStep(double percent) const;

  // Returns either |plugged_explicit_brightness_percent_| or
  // |unplugged_explicit_brightness_percent_| depending on |power_source_|.
  double GetExplicitBrightnessPercent() const;

  // Returns the brightness percent that should be used when the system is
  // in an undimmed state (|ambient_light_brightness_percent_| if
  // |use_ambient_light_| is true or a user- or policy-set level otherwise).
  double GetUndimmedBrightnessPercent() const;

  // Increases the explicitly-set brightness to the minimum visible level if
  // it's currently set to zero. Note that the brightness is left unchanged if
  // an external display is connected to avoid resizing the desktop, or if the
  // brightness was set to zero via a policy.
  void EnsureUserBrightnessIsNonzero();

  // Method that disables ambient light adjustments, updates the appropriate
  // |*_explicit_brightness_percent_| member, and updates the backlight's
  // brightness if needed. Returns true if the backlight's brightness was
  // changed.
  bool SetExplicitBrightnessPercent(
      double percent,
      TransitionStyle style,
      BrightnessChangeCause cause,
      PowerSource power_source);

  // Updates the current brightness after assessing the current state
  // (based on |power_source_|, |dimmed_for_inactivity_|, etc.).  Should be
  // called whenever the state changes.
  void UpdateState();

  // If the display is currently in the undimmed state, calls
  // ApplyBrightnessPercent() to update the backlight brightness.  Returns
  // true if the brightness was changed.
  bool UpdateUndimmedBrightness(TransitionStyle style,
                                BrightnessChangeCause cause);

  // Sets |backlight_|'s brightness to |percent| over |transition|.  If the
  // brightness changed, notifies |observers_| that the change was due to
  // |cause| and returns true.
  bool ApplyBrightnessPercent(double percent,
                              TransitionStyle transition,
                              BrightnessChangeCause cause);

  // Configures |backlight_| to resume from suspend at |resume_percent|.
  bool ApplyResumeBrightnessPercent(double resume_percent);

  // Updates displays to |state| after |delay| if |state| doesn't match
  // |display_power_state_|.  If another change has already been scheduled,
  // it will be aborted.
  void SetDisplayPower(chromeos::DisplayPowerState state,
                       base::TimeDelta delay);

  // Backlight used for dimming. Weak pointer.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Weak pointer.
  PrefsInterface* prefs_;

  // Used to turn displays on and off.
  system::DisplayPowerSetterInterface* display_power_setter_;

  scoped_ptr<AmbientLightHandler> ambient_light_handler_;

  scoped_ptr<Clock> clock_;

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

  // Time at which Init() was called.
  base::TimeTicks init_time_;

  // Indicates whether SetBrightnessPercentForAmbientLight() and
  // HandlePowerSourceChange() have been called yet.
  bool got_ambient_light_brightness_percent_;
  bool got_power_source_;

  // Has UpdateState() already set the initial state?
  bool already_set_initial_state_;

  // Number of ambient-light- and user-triggered brightness adjustments in the
  // current session.
  int als_adjustment_count_;
  int user_adjustment_count_;

  // Ambient-light-sensor-derived brightness percent supplied by
  // |ambient_light_handler_|.
  double ambient_light_brightness_percent_;

  // User- or policy-set brightness percent when AC is plugged or unplugged.
  double plugged_explicit_brightness_percent_;
  double unplugged_explicit_brightness_percent_;

  // True if the most-recently-received policy message requested a specific
  // brightness and no user adjustments have been made since then.
  bool using_policy_brightness_;

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

  // If true, then suggestions from |ambient_light_handler_| are used.
  // False if |ambient_light_handler_| is NULL, kDisableALSPref was set, or
  // the user has manually set the brightness.
  bool use_ambient_light_;

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

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_INTERNAL_BACKLIGHT_CONTROLLER_H_
