// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_BACKLIGHT_CONTROLLER_STUB_H_
#define POWER_MANAGER_POWERD_POLICY_BACKLIGHT_CONTROLLER_STUB_H_

#include <base/macros.h>
#include <base/observer_list.h>

#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/backlight_controller_observer.h"

namespace power_manager {
namespace policy {

// policy::BacklightController implementation that returns dummy values.
class BacklightControllerStub : public policy::BacklightController {
 public:
  BacklightControllerStub();
  virtual ~BacklightControllerStub();

  void set_percent(double percent) { percent_ = percent; }
  void set_num_als_adjustments(int num) { num_als_adjustments_ = num; }
  void set_num_user_adjustments(int num) { num_user_adjustments_ = num; }

  // policy::BacklightController implementation:
  void AddObserver(policy::BacklightControllerObserver* observer) override;
  void RemoveObserver(policy::BacklightControllerObserver* observer) override;
  void HandlePowerSourceChange(PowerSource source) override {}
  void HandleDisplayModeChange(DisplayMode mode) override {}
  void HandleSessionStateChange(SessionState state) override {}
  void HandlePowerButtonPress() override {}
  void HandleUserActivity(UserActivityType type) override {}
  void HandlePolicyChange(const PowerManagementPolicy& policy) override {}
  void HandleChromeStart() override {}
  void SetDimmedForInactivity(bool dimmed) override {}
  void SetOffForInactivity(bool off) override {}
  void SetSuspended(bool suspended) override {}
  void SetShuttingDown(bool shutting_down) override {}
  void SetDocked(bool docked) override {}
  bool GetBrightnessPercent(double* percent) override;
  bool SetUserBrightnessPercent(double percent,
                                TransitionStyle style) override {
    return true;
  }
  bool IncreaseUserBrightness() override { return true; }
  bool DecreaseUserBrightness(bool allow_off) override { return true; }
  int GetNumAmbientLightSensorAdjustments() const override {
    return num_als_adjustments_;
  }
  int GetNumUserAdjustments() const override { return num_user_adjustments_; }

  // Notify |observers_| that the brightness has changed to |percent| due
  // to |cause|. Also updates |percent_|.
  void NotifyObservers(double percent, BrightnessChangeCause cause);

 private:
  ObserverList<BacklightControllerObserver> observers_;

  // Percent to be returned by GetBrightnessPercent().
  double percent_;

  // Counts to be returned by GetNum*Adjustments().
  int num_als_adjustments_;
  int num_user_adjustments_;

  DISALLOW_COPY_AND_ASSIGN(BacklightControllerStub);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_BACKLIGHT_CONTROLLER_STUB_H_
