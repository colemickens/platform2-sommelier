// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/wakeup_device_stub.h"

namespace power_manager {
namespace system {

WakeupDeviceStub::WakeupDeviceStub() : caused_last_wake_(false) {}

WakeupDeviceStub::~WakeupDeviceStub() = default;

void WakeupDeviceStub::PrepareForSuspend() {}

void WakeupDeviceStub::HandleResume() {}

bool WakeupDeviceStub::CausedLastWake() const {
  return caused_last_wake_;
}

}  // namespace system
}  // namespace power_manager
