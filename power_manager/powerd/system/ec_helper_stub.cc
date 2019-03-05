// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ec_helper_stub.h"

namespace power_manager {
namespace system {

EcHelperStub::EcHelperStub() : wakeup_as_tablet_allowed_(false) {}
EcHelperStub::~EcHelperStub() {}

bool EcHelperStub::IsWakeAngleSupported() {
  return true;
}

bool EcHelperStub::AllowWakeupAsTablet(bool enabled) {
  wakeup_as_tablet_allowed_ = enabled;
  return true;
}

bool EcHelperStub::IsWakeupAsTabletAllowed() {
  return wakeup_as_tablet_allowed_;
}

}  // namespace system
}  // namespace power_manager
