// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "power_manager/powerd/policy/backlight_controller.h"

namespace power_manager {

namespace system {
class DisplayPowerSetterInterface;
}  // namespace system

namespace policy {

// Controls the brightness of an external display on machines that lack internal
// displays.
class ExternalBacklightController : public BacklightController {
 public:
  explicit ExternalBacklightController(
      system::DisplayPowerSetterInterface* display_power_setter);
  virtual ~ExternalBacklightController();

  // Initializes the object.
  void Init();

  // BacklightController implementation:
  virtual void AddObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE;
  virtual void HandleDisplayModeChange(DisplayMode mode) OVERRIDE;
  virtual void HandleSessionStateChange(SessionState state) OVERRIDE;
  virtual void HandlePowerButtonPress() OVERRIDE;
  virtual void HandleUserActivity(UserActivityType type) OVERRIDE;
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

 private:
  // Turns displays on or off via |monitor_reconfigure_| as needed for
  // |off_for_inactivity_|, |suspended_|, and |shutting_down_|.
  void UpdateScreenPowerState();

  system::DisplayPowerSetterInterface* display_power_setter_;  // not owned

  ObserverList<BacklightControllerObserver> observers_;

  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool suspended_;
  bool shutting_down_;

  // Are the external displays currently turned off?
  bool currently_off_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklightController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_
