// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/backlight_controller_observer_stub.h"

namespace power_manager {
namespace policy {

BacklightControllerObserverStub::BacklightControllerObserverStub() {}

BacklightControllerObserverStub::~BacklightControllerObserverStub() {}

void BacklightControllerObserverStub::Clear() {
  changes_.clear();
}

void BacklightControllerObserverStub::OnBrightnessChange(
    double brightness_percent,
    BacklightBrightnessChange_Cause cause,
    BacklightController* source) {
  ChangeTuple change;
  change.percent = brightness_percent;
  change.cause = cause;
  change.source = source;
  changes_.push_back(change);
}

}  // namespace policy
}  // namespace power_manager
