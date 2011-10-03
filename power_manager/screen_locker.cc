// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/screen_locker.h"

#include "base/logging.h"
#include "power_manager/util.h"

namespace power_manager {

ScreenLocker::ScreenLocker()
    : use_xscreensaver_(false),
      locked_(false),
      lock_on_suspend_(false) {
}

void ScreenLocker::Init(bool use_xscreensaver, bool lock_on_suspend) {
  use_xscreensaver_ = use_xscreensaver;
  lock_on_suspend_ = lock_on_suspend;
}

void ScreenLocker::LockScreen() {
  LOG(INFO) << "Locking screen";
  if (use_xscreensaver_) {
    util::Launch("xscreensaver-command -lock");
  } else {
    util::SendSignalToSessionManager("LockScreen");
    last_lock_request_time_ = base::TimeTicks::Now();
  }
}

}  // namespace power_manager
