// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_SHUTDOWN_FROM_SUSPEND_INTERFACE_H_
#define POWER_MANAGER_POWERD_POLICY_SHUTDOWN_FROM_SUSPEND_INTERFACE_H_

namespace power_manager {
namespace policy {

// Holds the logic to shut down the device after prolonged non use.
//
// Responsible for setting an alarm for |kShutdownAfterSecPref| before every
// suspend if one is not already running.
// On dark resume this code will shut down the device instead of re-suspending
// if following conditions hold true:
//   1. Device has spent |kShutdownAfterSecPref| in suspend or in dark resume
//      without a full resume.
//   2. Device is not connected to line power.
// On full resume, the alarm is stopped and the state is reset.
class ShutdownFromSuspendInterface {
 public:
  enum class Action {
    // Suspend the system.
    SUSPEND = 0,
    // Shut the system down immediately.
    SHUT_DOWN,
  };
  ShutdownFromSuspendInterface() {}
  virtual ~ShutdownFromSuspendInterface() {}

  // Updates state in anticipation of the system suspending, returning the
  // action that should be performed.
  virtual Action PrepareForSuspendAttempt() = 0;
  // Called when device does a dark resume.
  virtual void HandleDarkResume() = 0;
  // Called when device does a full resume or on transitions from dark resume to
  // full resume.
  virtual void HandleFullResume() = 0;
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_SHUTDOWN_FROM_SUSPEND_INTERFACE_H_
