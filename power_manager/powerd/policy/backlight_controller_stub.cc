// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/backlight_controller_stub.h"

#include "base/logging.h"

namespace power_manager {
namespace policy {

BacklightControllerStub::BacklightControllerStub()
    : percent_(100.0),
      num_als_adjustments_(0),
      num_user_adjustments_(0) {}

BacklightControllerStub::~BacklightControllerStub() {}

void BacklightControllerStub::AddObserver(
    policy::BacklightControllerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BacklightControllerStub::RemoveObserver(
    policy::BacklightControllerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool BacklightControllerStub::GetBrightnessPercent(double* percent) {
  DCHECK(percent);
  *percent = percent_;
  return true;
}

void BacklightControllerStub::NotifyObservers(double percent,
                                              BrightnessChangeCause cause) {
  percent_ = percent;
  FOR_EACH_OBSERVER(BacklightControllerObserver, observers_,
                    OnBrightnessChanged(percent_, cause, this));
}

}  // namespace policy
}  // namespace power_manager
