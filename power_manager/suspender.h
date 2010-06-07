// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_SUSPENDER_H_
#define POWER_MANAGER_SUSPENDER_H_

#include <gdk/gdkx.h>

#include "power_manager/screen_locker.h"

namespace power_manager {

class Suspender {
 public:
  explicit Suspender(ScreenLocker* locker);

  // Suspend the computer, locking the screen first.
  void RequestSuspend();

  // Check whether the computer should be suspended. Before calling this
  // method, the screen should be locked.
  void CheckSuspend();

 private:
  // Suspend the computer. Before calling this method, the screen should
  // be locked.
  void Suspend();

  // Check whether the computer should be suspended. Before calling this
  // method, the screen should be locked. Always returns false.
  static gboolean CheckSuspendTimeout(gpointer data);

  // Reference to ScreenLocker object.
  ScreenLocker* locker_;

  // Whether the computer should be suspended soon.
  bool suspend_requested_;

  DISALLOW_COPY_AND_ASSIGN(Suspender);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_SUSPENDER_H_
