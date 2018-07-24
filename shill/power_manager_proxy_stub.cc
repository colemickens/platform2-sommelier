// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager_proxy_stub.h"

namespace shill {

PowerManagerProxyStub::PowerManagerProxyStub() {}

bool PowerManagerProxyStub::RegisterSuspendDelay(
    base::TimeDelta /*timeout*/,
    const std::string& /*description*/,
    int* /*delay_id_out*/) {
  // STUB IMPLEMENTATION.
  return false;
}

bool PowerManagerProxyStub::UnregisterSuspendDelay(int /*delay_id*/) {
  // STUB IMPLEMENTATION.
  return false;
}

bool PowerManagerProxyStub::ReportSuspendReadiness(int /*delay_id*/,
                                                   int /*suspend_id*/) {
  // STUB IMPLEMENTATION.
  return false;
}

bool PowerManagerProxyStub::RegisterDarkSuspendDelay(
    base::TimeDelta /*timeout*/,
    const std::string& /*description*/,
    int* /*delay_id_out*/) {
  // STUB IMPLEMENTATION.
  return false;
}

bool PowerManagerProxyStub::UnregisterDarkSuspendDelay(int /*delay_id*/) {
  // STUB IMPLEMENTATION.
  return false;
}

bool PowerManagerProxyStub::ReportDarkSuspendReadiness(int /*delay_id*/,
                                                       int /*suspend_id*/) {
  // STUB IMPLEMENTATION.
  return false;
}

bool PowerManagerProxyStub::RecordDarkResumeWakeReason(
    const std::string& /*wake_reason*/) {
  // STUB IMPLEMENTATION.
  return false;
}

}  // namespace shill
