// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/cros_ec_helper_stub.h"

namespace power_manager {
namespace system {

CrosEcHelperStub::CrosEcHelperStub() : wakeup_as_tablet_allowed_(false) {}
CrosEcHelperStub::~CrosEcHelperStub() {}

bool CrosEcHelperStub::IsWakeAngleSupported() {
  return true;
}

bool CrosEcHelperStub::AllowWakeupAsTablet(bool enabled) {
  wakeup_as_tablet_allowed_ = enabled;
  return true;
}

bool CrosEcHelperStub::IsWakeupAsTabletAllowed() {
  return wakeup_as_tablet_allowed_;
}

}  // namespace system
}  // namespace power_manager
