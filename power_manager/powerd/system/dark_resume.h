// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_H_
#define POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_H_

#include <base/macros.h>

#include "power_manager/powerd/system/dark_resume_interface.h"

namespace power_manager {

class PrefsInterface;

namespace system {

class InputWatcherInterface;

// Newer implementation of dark resume. Uses per device (peripheral) wakeup
// count to identify the wake source.
class DarkResume : public DarkResumeInterface {
 public:
  DarkResume();
  ~DarkResume() override;

  // Reads preferences on whether dark resume is enabled.
  void Init(PrefsInterface* prefs, InputWatcherInterface* input_watcher);

  // DarkResumeInterface implementation:
  void PrepareForSuspendRequest() override;
  void UndoPrepareForSuspendRequest() override;
  Action GetActionForSuspendAttempt() override;
  void HandleSuccessfulResume() override;
  bool InDarkResume() override;
  bool IsEnabled() override;
  bool ExitDarkResume() override;

 private:
  // Are we currently in dark resume?
  bool in_dark_resume_ = false;

  // Is dark resume enabled?
  bool enabled_ = false;

  PrefsInterface* prefs_;  // weak

  InputWatcherInterface* input_watcher_;  // weak

  DISALLOW_COPY_AND_ASSIGN(DarkResume);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_H_
