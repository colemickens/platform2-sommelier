// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display_power_setter_stub.h"

namespace power_manager {
namespace system {

DisplayPowerSetterStub::DisplayPowerSetterStub()
    : state_(chromeos::DISPLAY_POWER_ALL_ON),
      num_calls_(0) {
}

DisplayPowerSetterStub::~DisplayPowerSetterStub() {}

void DisplayPowerSetterStub::SetDisplayPower(chromeos::DisplayPowerState state,
                                             base::TimeDelta delay) {
  state_ = state;
  delay_ = delay;
  num_calls_++;
}

}  // system
}  // power_manager
