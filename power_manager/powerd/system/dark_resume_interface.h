// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_INTERFACE_H_

namespace base {
class TimeDelta;
}  // namespace base

namespace power_manager {
namespace system {

// Returns information related to "dark resume", a mode where the system wakes
// without user interaction to display notifications, or to show alarms.
class DarkResumeInterface {
 public:
  enum class Action {
    // Suspend the system.
    SUSPEND = 0,
    // Shut the system down immediately.
    SHUT_DOWN,
  };

  DarkResumeInterface() {}
  virtual ~DarkResumeInterface() {}

  // These methods bracket each suspend request. PrepareForSuspendRequest will
  // take a snapshot of wake counts before suspend to be compared to the wake
  // counts after resume to identify the wake source.
  virtual void PrepareForSuspendRequest() = 0;
  virtual void UndoPrepareForSuspendRequest() = 0;

  // Returns the action that should actually be performed in response to an
  // decision to attempt to suspend or resuspend the device. We will typically
  // suspend in this case, but we may sometimes choose to perform other actions
  // (e.g. shutting down if the device has been suspended for a long time).
  virtual Action GetActionForSuspendAttempt() = 0;

  // Reads the system state to see if it's in a dark resume.
  virtual void HandleSuccessfulResume() = 0;

  // Returns true if the system is currently in dark resume.
  virtual bool InDarkResume() = 0;

  // Returns true if dark resume is enabled on the system.
  virtual bool IsEnabled() = 0;

  // Exits dark resume so that the system can transition to fully resumed.
  virtual void ExitDarkResume() = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_INTERFACE_H_
