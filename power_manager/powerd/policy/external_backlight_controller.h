// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/observer_list.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/system/display/display_watcher_observer.h"

namespace power_manager {

namespace system {
struct DisplayInfo;
class DisplayPowerSetterInterface;
class DisplayWatcherInterface;
class ExternalDisplay;
}  // namespace system

namespace policy {

// Controls the brightness of an external display on machines that lack internal
// displays.
class ExternalBacklightController : public BacklightController,
                                    public system::DisplayWatcherObserver {
 public:
  // Amount the brightness will be adjusted up or down in response to a user
  // request, as a linearly-calculated percent in the range [0.0, 100.0].
  const static double kBrightnessAdjustmentPercent;

  ExternalBacklightController();
  virtual ~ExternalBacklightController();

  // Initializes the object. Ownership of |display_watcher| and
  // |display_power_setter| remain with the caller.
  void Init(system::DisplayWatcherInterface* display_watcher,
            system::DisplayPowerSetterInterface* display_power_setter);

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

  // system::DisplayWatcherObserver implementation:
  virtual void OnDisplaysChanged(
      const std::vector<system::DisplayInfo>& displays) OVERRIDE;

 private:
  // Turns displays on or off via |monitor_reconfigure_| as needed for
  // |off_for_inactivity_|, |suspended_|, and |shutting_down_|.
  void UpdateScreenPowerState();

  // Sends notifications to |observers_| about the current brightness level.
  void NotifyObservers();

  // Updates |external_displays_| for |displays|.
  void UpdateDisplays(const std::vector<system::DisplayInfo>& displays);

  // Adjusts |external_displays_| by |percent_offset|, a linearly-calculated
  // percent in the range [-100.0, 100.0].
  void AdjustBrightnessByPercent(double percent_offset);

  system::DisplayWatcherInterface* display_watcher_;  // weak
  system::DisplayPowerSetterInterface* display_power_setter_;  // weak

  ObserverList<BacklightControllerObserver> observers_;

  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool suspended_;
  bool shutting_down_;

  // Are the external displays currently turned off?
  bool currently_off_;

  // Map from DRM device directories to ExternalDisplay objects for controlling
  // the corresponding displays.
  typedef std::map<base::FilePath, linked_ptr<system::ExternalDisplay> >
      ExternalDisplayMap;
  ExternalDisplayMap external_displays_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklightController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_
