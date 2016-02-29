// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ec_wakeup_helper_stub.h"

namespace power_manager {
namespace system {

EcWakeupHelperStub::EcWakeupHelperStub() : wakeup_as_tablet_allowed_(false) {}
EcWakeupHelperStub::~EcWakeupHelperStub() {}

bool EcWakeupHelperStub::IsSupported() {
  return true;
}

bool EcWakeupHelperStub::AllowWakeupAsTablet(bool enabled) {
  wakeup_as_tablet_allowed_ = enabled;
  return true;
}

bool EcWakeupHelperStub::IsWakeupAsTabletAllowed() {
  return wakeup_as_tablet_allowed_;
}

}  // namespace system
}  // namespace power_manager
