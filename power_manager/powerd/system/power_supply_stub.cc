// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/power_supply_stub.h"

namespace power_manager {
namespace system {

PowerSupplyStub::PowerSupplyStub() : refresh_result_(true) {}

PowerSupplyStub::~PowerSupplyStub() {}

void PowerSupplyStub::AddObserver(PowerSupplyObserver* observer) {
  CHECK(observer);
}

void PowerSupplyStub::RemoveObserver(PowerSupplyObserver* observer) {
  CHECK(observer);
}

PowerStatus PowerSupplyStub::GetPowerStatus() const {
  return status_;
}

bool PowerSupplyStub::RefreshImmediately() {
  return refresh_result_;
}

void PowerSupplyStub::SetSuspended(bool suspended) {}

}  // namespace system
}  // namespace power_manager
