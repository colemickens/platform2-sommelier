// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_
#define POWER_MANAGER_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_

#include <utility>
#include <vector>

#include <base/basictypes.h>

#include "power_manager/backlight_controller.h"

namespace power_manager {

// Simple helper class that logs backlight brightness changes.
class MockBacklightControllerObserver : public BacklightControllerObserver {
 public:
  MockBacklightControllerObserver() {}
  virtual ~MockBacklightControllerObserver() {}

  std::vector<std::pair<double, BrightnessChangeCause> > changes() const {
    return changes_;
  }

  void Clear() {
    changes_.clear();
  }

  // BacklightControllerObserver implementation:
  virtual void OnScreenBrightnessChanged(double brightness_percent,
                                         BrightnessChangeCause cause) {
    changes_.push_back(std::make_pair(brightness_percent, cause));
  }

 private:
  // Received changes, in oldest-to-newest order.
  std::vector<std::pair<double, BrightnessChangeCause> > changes_;

  DISALLOW_COPY_AND_ASSIGN(MockBacklightControllerObserver);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MOCK_BACKLIGHT_CONTROLLER_OBSERVER_H_
