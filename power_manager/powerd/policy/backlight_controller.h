// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_BACKLIGHT_CONTROLLER_H_

#include <base/macros.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/backlight_interface.h"

namespace power_manager {

class PowerManagementPolicy;

namespace policy {

class BacklightControllerObserver;

// Interface implemented by classes that control the backlight.
class BacklightController {
 public:
  // Different ways to transition between brightness levels.
  enum class Transition {
    INSTANT,
    FAST,
    SLOW,
  };

  BacklightController() {}
  virtual ~BacklightController() {}

  // Adds or removes an observer.
  virtual void AddObserver(BacklightControllerObserver* observer) = 0;
  virtual void RemoveObserver(BacklightControllerObserver* observer) = 0;

  // Handles the system's source of power being changed.
  virtual void HandlePowerSourceChange(PowerSource source) = 0;

  // Handles the display mode changing.
  virtual void HandleDisplayModeChange(DisplayMode mode) = 0;

  // Handles the session state changing.
  virtual void HandleSessionStateChange(SessionState state) = 0;

  // Handles the power button being pressed.
  virtual void HandlePowerButtonPress() = 0;

  // Handles user activity.  Note that screen dimming and undimming in
  // response to user inactivity/activity are controlled via
  // SetDimmingForInactivity(); this method should only be used for e.g.
  // ensuring that the user-set brightness is nonzero.
  virtual void HandleUserActivity(UserActivityType type) = 0;

  // Handles a notification of video activity. This is called periodically while
  // video is ongoing.
  virtual void HandleVideoActivity(bool is_fullscreen) = 0;

  // Handles the user's hands moving to or away from the touchpad or keyboard.
  virtual void HandleHoverStateChange(bool hovering) = 0;

  // Handles the system (if convertible) entering or exiting tablet mode.
  virtual void HandleTabletModeChange(TabletMode mode) = 0;

  // Handles an updated policy.
  virtual void HandlePolicyChange(const PowerManagementPolicy& policy) = 0;

  // Handles Chrome starting (as detected by the ownership of its D-Bus object
  // changing).
  virtual void HandleDisplayServiceStart() = 0;

  // Sets whether the backlight should be immediately dimmed in response to
  // user inactivity.  Note that other states take precedence over this
  // one, e.g. the backlight will be turned off if SetOffForInactivity(true)
  // is called after SetDimmedForInactivity(true).
  virtual void SetDimmedForInactivity(bool dimmed) = 0;

  // Sets whether the backlight should be immediately turned off in
  // response to user inactivity.
  virtual void SetOffForInactivity(bool off) = 0;

  // Sets whether the backlight should be prepared for the system being
  // suspended.
  virtual void SetSuspended(bool suspended) = 0;

  // Sets whether the backlight should be prepared for imminent shutdown.
  virtual void SetShuttingDown(bool shutting_down) = 0;

  // Sets whether the system is in "docked mode" (external display
  // connected while lid closed).
  virtual void SetDocked(bool docked) = 0;

  // Forces the backlight off.
  virtual void SetForcedOff(bool forced_off) = 0;

  // Returns the state most recently passed to SetForcedOff.
  virtual bool GetForcedOff() = 0;

  // Gets the brightness that the backlight currently using or
  // transitioning to, in the range [0.0, 100.0].
  virtual bool GetBrightnessPercent(double* percent) = 0;

  // Sets the brightness of the backlight in the range [0.0, 100.0] in
  // response to a user request.  Note that the change may not take effect
  // immediately (e.g. the screen may be dimmed or turned off).  Returns
  // true if the brightness was changed.
  virtual bool SetUserBrightnessPercent(double percent,
                                        Transition transition) = 0;

  // Increases the brightness level of the backlight by one step in
  // response to a user request.  Returns true if the brightness was
  // changed.
  virtual bool IncreaseUserBrightness() = 0;

  // Decreases the brightness level of the backlight by one step in
  // response to a user request.  Returns true if the brightness was
  // changed.
  //
  // If |allow_off| is false, the backlight will never be entirely turned off.
  // This should be used with on-screen controls to prevent their becoming
  // impossible for the user to see.
  virtual bool DecreaseUserBrightness(bool allow_off) = 0;

  // Returns the number of times that the backlight has been adjusted as a
  // result of ALS readings or user requests during the current session.
  virtual int GetNumAmbientLightSensorAdjustments() const = 0;
  virtual int GetNumUserAdjustments() const = 0;

  // Converts between a (possibly-non-linear) brightness percent in the range
  // [0.0, 100.0] and a hardware-specific brightness level (as exposed by the
  // backlight driver).
  virtual int64_t PercentToLevel(double percent) const = 0;
  virtual double LevelToPercent(int64_t level) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_BACKLIGHT_CONTROLLER_H_
