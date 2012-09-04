// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_
#define POWER_MANAGER_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_

#include <base/basictypes.h>

#include <utility>
#include <vector>

#include "power_manager/backlight_controller.h"

namespace power_manager {

// Simple helper class that logs backlight brightness changes.
class MockBacklightControllerObserver : public BacklightControllerObserver {
 public:
  struct ChangeTuple {
    double percent;
    BrightnessChangeCause cause;
    BacklightController* source;
  };  // struct ChangeTuple

  MockBacklightControllerObserver() {}
  virtual ~MockBacklightControllerObserver() {}

  std::vector<struct ChangeTuple> changes() const {
    return changes_;
  }

  void Clear() {
    changes_.clear();
  }

  // BacklightControllerObserver implementation:
  virtual void OnBrightnessChanged(double brightness_percent,
                                   BrightnessChangeCause cause,
                                   BacklightController* source) {
    struct ChangeTuple change;
    change.percent = brightness_percent;
    change.cause = cause;
    change.source = source;
    changes_.push_back(change);
  }

 private:
  // Received changes, in oldest-to-newest order.
  std::vector<struct ChangeTuple> changes_;

  DISALLOW_COPY_AND_ASSIGN(MockBacklightControllerObserver);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_
