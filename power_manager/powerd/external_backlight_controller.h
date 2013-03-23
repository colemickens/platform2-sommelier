// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_EXTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_EXTERNAL_BACKLIGHT_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/system/backlight_interface.h"

namespace power_manager {

namespace system {
class BacklightInterface;
class DisplayPowerSetterInterface;
}  // namespace system

// Controls the brightness of an external display on machines that lack internal
// displays.
class ExternalBacklightController : public BacklightController,
                                    public system::BacklightInterfaceObserver {
 public:
  ExternalBacklightController(
      system::BacklightInterface* backlight,
      system::DisplayPowerSetterInterface* display_power_setter);
  virtual ~ExternalBacklightController();

  // Initializes the object.
  bool Init();

  // BacklightController implementation:
  virtual void AddObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightControllerObserver* observer) OVERRIDE;
  virtual bool GetBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetUserBrightnessPercent(double percent, TransitionStyle style)
      OVERRIDE;
  virtual bool IncreaseUserBrightness(bool only_if_zero) OVERRIDE;
  virtual bool DecreaseUserBrightness(bool allow_off) OVERRIDE;
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE;
  virtual void SetDimmedForInactivity(bool dimmed) OVERRIDE;
  virtual void SetOffForInactivity(bool off) OVERRIDE;
  virtual void SetSuspended(bool suspended) OVERRIDE;
  virtual void SetShuttingDown(bool shutting_down) OVERRIDE;
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE;
  virtual int GetNumUserAdjustments() const OVERRIDE;

  // BacklightInterfaceObserver implementation:
  virtual void OnBacklightDeviceChanged() OVERRIDE;

 private:
  // For PercentToLevel().
  friend class ExternalBacklightControllerTest;

  double LevelToPercent(int64 level);
  int64 PercentToLevel(double percent);

  // Adjusts the user-requested brightness by |percent_offset|.
  // |allow_off| and |only_if_zero| correspond to the identically-named
  // parameters to IncreaseUserBrightness() and DecreaseUserBrightness().
  bool AdjustUserBrightnessByOffset(double percent_offset,
                                    bool allow_off,
                                    bool only_if_zero);

  // Turns displays on or off via |monitor_reconfigure_| as needed for
  // |off_for_inactivity_|, |suspended_|, and |shutting_down_|.
  void UpdateScreenPowerState();

  system::BacklightInterface* backlight_;  // not owned
  system::DisplayPowerSetterInterface* display_power_setter_;  // not owned

  ObserverList<BacklightControllerObserver> observers_;

  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool suspended_;
  bool shutting_down_;

  // Maximum brightness level exposed by the current display.
  // 0 is always the minimum.
  int64 max_level_;

  // Are the external displays currently turned off?
  bool currently_off_;

  // Number of times that we've applied user-initiated brightness requests.
  int num_user_adjustments_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklightController);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_EXTERNAL_BACKLIGHT_CONTROLLER_H_
