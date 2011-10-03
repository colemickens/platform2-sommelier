// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_SCREEN_LOCKER_H_
#define POWER_MANAGER_SCREEN_LOCKER_H_

#include "base/basictypes.h"
#include "base/time.h"

namespace power_manager {

class ScreenLocker {
 public:
  ScreenLocker();

  // Initializer. If |use_xscreensaver| is set, use xscreensaver to
  // lock the screen. Otherwise, we use Chrome.
  void Init(bool use_xscreensaver, bool lock_on_idle_suspend);

  // Ask the session manager to lock the screen.
  // Note that |locked_| won't be updated immediately.
  void LockScreen();

  bool is_locked() const { return locked_; }
  void set_locked(bool locked) { locked_ = locked; }
  base::TimeTicks last_lock_request_time() const {
    return last_lock_request_time_;
  }
  bool lock_on_suspend_enabled() const { return lock_on_suspend_; }

 private:
  // If |use_xscreensaver| is set, use xscreensaver to lock the screen.
  // Otherwise, we use Chrome to lock the screen.
  bool use_xscreensaver_;

  // Whether the screen is currently locked.
  // Note that this is updated in response to ScreenIsLocked and
  // ScreenIsUnlocked messages from Chrome, which are received asynchronously
  // after a request is sent by LockScreen().
  bool locked_;

  // Time at which we last asked the session manager to lock the screen.
  base::TimeTicks last_lock_request_time_;

  // Whether the screenlocker should be invoked when idle, or when suspended
  bool lock_on_suspend_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_SCREEN_LOCKER_H_
