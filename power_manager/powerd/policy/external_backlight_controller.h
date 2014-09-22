// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_

#include <map>
#include <vector>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <base/memory/linked_ptr.h>
#include <base/observer_list.h>

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
  ExternalBacklightController();
  virtual ~ExternalBacklightController();

  // Initializes the object. Ownership of |display_watcher| and
  // |display_power_setter| remain with the caller.
  void Init(system::DisplayWatcherInterface* display_watcher,
            system::DisplayPowerSetterInterface* display_power_setter);

  // BacklightController implementation:
  void AddObserver(BacklightControllerObserver* observer) override;
  void RemoveObserver(BacklightControllerObserver* observer) override;
  void HandlePowerSourceChange(PowerSource source) override;
  void HandleDisplayModeChange(DisplayMode mode) override;
  void HandleSessionStateChange(SessionState state) override;
  void HandlePowerButtonPress() override;
  void HandleUserActivity(UserActivityType type) override;
  void HandlePolicyChange(const PowerManagementPolicy& policy) override;
  void HandleChromeStart() override;
  void SetDimmedForInactivity(bool dimmed) override;
  void SetOffForInactivity(bool off) override;
  void SetSuspended(bool suspended) override;
  void SetShuttingDown(bool shutting_down) override;
  void SetDocked(bool docked) override;
  bool GetBrightnessPercent(double* percent) override;
  bool SetUserBrightnessPercent(double percent, TransitionStyle style) override;
  bool IncreaseUserBrightness() override;
  bool DecreaseUserBrightness(bool allow_off) override;
  int GetNumAmbientLightSensorAdjustments() const override;
  int GetNumUserAdjustments() const override;

  // system::DisplayWatcherObserver implementation:
  void OnDisplaysChanged(
      const std::vector<system::DisplayInfo>& displays) override;

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
  typedef std::map<base::FilePath, linked_ptr<system::ExternalDisplay>>
      ExternalDisplayMap;
  ExternalDisplayMap external_displays_;

  // Number of times the user has requested that the brightness be changed in
  // the current session.
  int num_brightness_adjustments_in_session_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBacklightController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_EXTERNAL_BACKLIGHT_CONTROLLER_H_
