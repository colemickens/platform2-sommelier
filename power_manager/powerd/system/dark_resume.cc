// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dark_resume.h"

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/powerd/system/input_watcher_interface.h"

namespace power_manager {
namespace system {

DarkResume::DarkResume() {}

DarkResume::~DarkResume() {}

void DarkResume::Init(PrefsInterface* prefs,
                      InputWatcherInterface* input_watcher) {
  DCHECK(prefs);
  DCHECK(input_watcher);

  prefs_ = prefs;
  input_watcher_ = input_watcher;

  bool disable = false;
  enabled_ = (!prefs_->GetBool(kDisableDarkResumePref, &disable) || !disable);
  LOG(INFO) << "Dark resume user space " << (enabled_ ? "enabled" : "disabled");
}

void DarkResume::PrepareForSuspendRequest() {
  // We want to keep track of wakeup count of devices even when dark resume is
  // not enabled as this will help in identifying the wake source when debugging
  // spurious wakes.
  input_watcher_->PrepareForSuspendRequest();
}

void DarkResume::UndoPrepareForSuspendRequest() {
  in_dark_resume_ = false;
}

void DarkResume::GetActionForSuspendAttempt(Action* action,
                                            base::TimeDelta* suspend_duration) {
  DCHECK(action);
  DCHECK(suspend_duration);
  *action = Action::SUSPEND;
  *suspend_duration = base::TimeDelta();
}

void DarkResume::HandleSuccessfulResume() {
  in_dark_resume_ = false;

  input_watcher_->HandleResume();
  if (input_watcher_->InputDeviceCausedLastWake()) {
    VLOG(1) << "User triggered wake";
  } else {
    VLOG(1) << "Wake not triggered by user";
    if (enabled_) {
      LOG(INFO) << "In dark resume";
      in_dark_resume_ = true;
    }
  }
}

bool DarkResume::InDarkResume() {
  return in_dark_resume_;
}

bool DarkResume::IsEnabled() {
  return enabled_;
}

bool DarkResume::ExitDarkResume() {
  LOG(INFO) << "Transitioning from dark resume to full resume";
  in_dark_resume_ = false;
  return true;
}
}  // namespace system
}  // namespace power_manager
