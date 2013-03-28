// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/backlight_interface.h"

namespace power_manager {

class BacklightControllerObserver;

// Interface implemented by classes that control the backlight.
class BacklightController {
 public:
  // Possible causes of changes to the backlight brightness level.
  enum BrightnessChangeCause {
    // The brightness was changed automatically (in response to e.g. an idle
    // transition or AC getting plugged or unplugged).
    BRIGHTNESS_CHANGE_AUTOMATED,

    // The user requested that the brightness be changed.
    BRIGHTNESS_CHANGE_USER_INITIATED,
  };

  // Different ways to transition between brightness levels.
  enum TransitionStyle {
    TRANSITION_INSTANT,
    TRANSITION_FAST,
    TRANSITION_SLOW,
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

  // Gets the brightness that the backlight currently using or
  // transitioning to, in the range [0.0, 100.0].
  virtual bool GetBrightnessPercent(double* percent) = 0;

  // Sets the brightness of the backlight in the range [0.0, 100.0] in
  // response to a user request.  Note that the change may not take effect
  // immediately (e.g. the screen may be dimmed or turned off).  Returns
  // true if the brightness was changed.
  virtual bool SetUserBrightnessPercent(double percent,
                                        TransitionStyle style) = 0;

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
  // result of ALS readings or user requests.
  virtual int GetNumAmbientLightSensorAdjustments() const = 0;
  virtual int GetNumUserAdjustments() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_BACKLIGHT_CONTROLLER_H_
