// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "power_manager/backlight_interface.h"

namespace power_manager {

class AmbientLightSensor;
class MonitorReconfigure;

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

// Possible causes of changes to the backlight brightness level.
enum BrightnessChangeCause {
  // The brightness was changed automatically (in response to e.g. an idle
  // transition or AC getting plugged or unplugged).
  BRIGHTNESS_CHANGE_AUTOMATED,

  // The user requested that the brightness be changed.
  BRIGHTNESS_CHANGE_USER_INITIATED,
};

// Different ways to transition brightness levels.
enum TransitionStyle {
  TRANSITION_INSTANT,
  TRANSITION_FAST,
  TRANSITION_SLOW,
};

class BacklightController;

// Interface for observing changes made by the backlight controller.
class BacklightControllerObserver {
 public:
  // Invoked when the brightness level is changed.  |brightness_percent| is the
  // current brightness in the range [0, 100].
  virtual void OnBrightnessChanged(double brightness_percent,
                                   BrightnessChangeCause cause,
                                   BacklightController* source) {}

 protected:
  virtual ~BacklightControllerObserver() {}
};

// Interface implemented by classes that control the backlight.
class BacklightController : public BacklightInterfaceObserver {
 public:
  BacklightController() {}
  virtual ~BacklightController() {}

  // Initialize the object.  Note that this method is also reinvoked when the
  // backlight device changes.
  virtual bool Init() = 0;

  virtual void SetAmbientLightSensor(AmbientLightSensor* sensor) = 0;
  virtual void SetMonitorReconfigure(
      MonitorReconfigure* monitor_reconfigure) = 0;
  virtual void SetObserver(BacklightControllerObserver* observer) = 0;

  // Get the brightness that we're currently transitioning to, in the range [0,
  // 100].  Use GetCurrentBrightnessPercent() to query the instantaneous
  // brightness.
  virtual double GetTargetBrightnessPercent() = 0;

  // Get the current brightness of the backlight in the range [0, 100].
  // We may be in the process of smoothly transitioning to a different level.
  virtual bool GetCurrentBrightnessPercent(double* percent) = 0;

  // Set the current brightness of the backlight in the range [0, 100].
  // Returns true if the brightness was changed, false otherwise.
  virtual bool SetCurrentBrightnessPercent(double percent,
                                           BrightnessChangeCause cause,
                                           TransitionStyle style) = 0;

  // Increase the brightness level of the backlight by one step.
  // Returns true if the brightness was changed, false otherwise.
  virtual bool IncreaseBrightness(BrightnessChangeCause cause) = 0;

  // Decrease the brightness level of the backlight by one step.
  // Returns true if the brightness was changed, false otherwise.
  //
  // If |allow_off| is false, the backlight will never be entirely turned off.
  // This should be used with on-screen controls to prevent their becoming
  // impossible for the user to see.
  virtual bool DecreaseBrightness(bool allow_off,
                                  BrightnessChangeCause cause) = 0;

  // Turn the backlight on or off.  Returns true if the state was successfully
  // changed.
  virtual bool SetPowerState(PowerState state) = 0;

  // Get the previously-set state.
  virtual PowerState GetPowerState() const = 0;

  // Mark the computer as plugged or unplugged, and adjust the brightness
  // appropriately.  Returns true if the brightness was set and false
  // otherwise.
  virtual bool OnPlugEvent(bool is_plugged) = 0;

  // Update the brightness offset that is applied based on the current amount of
  // ambient light.
  virtual void SetAlsBrightnessOffsetPercent(double percent) = 0;

  // Determine whether the user has manually turned backlight down to zero.
  virtual bool IsBacklightActiveOff() = 0;

  // Get the number of times that the backlight has been adjusted as a result of
  // ALS readings or user requests.
  virtual int GetNumAmbientLightSensorAdjustments() const = 0;
  virtual int GetNumUserAdjustments() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_BACKLIGHT_CONTROLLER_H_
