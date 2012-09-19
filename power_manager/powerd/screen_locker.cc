// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/screen_locker.h"

#include "base/logging.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"

namespace power_manager {

ScreenLocker::ScreenLocker()
    : locked_(false),
      lock_on_suspend_(false) {
}

void ScreenLocker::Init(bool lock_on_suspend) {
  lock_on_suspend_ = lock_on_suspend;
}

void ScreenLocker::LockScreen() {
  LOG(INFO) << "Locking screen";
  util::SendSignalToSessionManager("LockScreen");
  last_lock_request_time_ = base::TimeTicks::Now();
}

}  // namespace power_manager
