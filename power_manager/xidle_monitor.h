// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_XIDLE_MONITOR_H_
#define POWER_MANAGER_XIDLE_MONITOR_H_

#include "base/basictypes.h"

namespace power_manager {

// Interface for monitoring idle events
class XIdleMonitor {
 public:
  // Idle event handler. This handler should be called when the user either
  // becomes newly idle (due to exceeding an idle timeout) or is no longer
  // idle.
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) = 0;

 protected:
  virtual ~XIdleMonitor() {}
};

}  // namespace power_manager

#endif  // POWER_MANAGER_XIDLE_MONITOR_H_
