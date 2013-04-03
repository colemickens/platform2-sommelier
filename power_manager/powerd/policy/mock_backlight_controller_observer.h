// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_
#define POWER_MANAGER_POWERD_POLICY_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_

#include <base/basictypes.h>

#include <utility>
#include <vector>

#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/backlight_controller_observer.h"

namespace power_manager {
namespace policy {

// Simple helper class that logs backlight brightness changes.
class MockBacklightControllerObserver : public BacklightControllerObserver {
 public:
  struct ChangeTuple {
    double percent;
    BacklightController::BrightnessChangeCause cause;
    BacklightController* source;
  };  // struct ChangeTuple

  MockBacklightControllerObserver() {}
  virtual ~MockBacklightControllerObserver() {}

  std::vector<ChangeTuple> changes() const {
    return changes_;
  }

  void Clear() {
    changes_.clear();
  }

  // BacklightControllerObserver implementation:
  virtual void OnBrightnessChanged(
      double brightness_percent,
      BacklightController::BrightnessChangeCause cause,
      BacklightController* source) {
    ChangeTuple change;
    change.percent = brightness_percent;
    change.cause = cause;
    change.source = source;
    changes_.push_back(change);
  }

 private:
  // Received changes, in oldest-to-newest order.
  std::vector<ChangeTuple> changes_;

  DISALLOW_COPY_AND_ASSIGN(MockBacklightControllerObserver);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_
