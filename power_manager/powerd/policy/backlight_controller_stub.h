// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/observer_list.h"
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
  virtual void AddObserver(
      policy::BacklightControllerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      policy::BacklightControllerObserver* observer) OVERRIDE;
  virtual void HandlePowerSourceChange(PowerSource source) OVERRIDE {}
  virtual void HandleDisplayModeChange(DisplayMode mode) OVERRIDE {}
  virtual void HandleSessionStateChange(SessionState state) OVERRIDE {}
  virtual void HandlePowerButtonPress() OVERRIDE {}
  virtual void HandleUserActivity(UserActivityType type) OVERRIDE {}
  virtual void HandlePolicyChange(const PowerManagementPolicy& policy)
      OVERRIDE {}
  virtual void HandleChromeStart() OVERRIDE {}
  virtual void SetDimmedForInactivity(bool dimmed) OVERRIDE {}
  virtual void SetOffForInactivity(bool off) OVERRIDE {}
  virtual void SetSuspended(bool suspended) OVERRIDE {}
  virtual void SetShuttingDown(bool shutting_down) OVERRIDE {}
  virtual void SetDocked(bool docked) OVERRIDE {}
  virtual bool GetBrightnessPercent(double* percent) OVERRIDE;
  virtual bool SetUserBrightnessPercent(double percent, TransitionStyle style)
      OVERRIDE {
    return true;
  }
  virtual bool IncreaseUserBrightness() OVERRIDE { return true; }
  virtual bool DecreaseUserBrightness(bool allow_off) OVERRIDE { return true; }
  virtual int GetNumAmbientLightSensorAdjustments() const OVERRIDE {
    return num_als_adjustments_;
  }
  virtual int GetNumUserAdjustments() const OVERRIDE {
    return num_user_adjustments_;
  }

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
