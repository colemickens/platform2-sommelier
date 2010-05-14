// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_DAEMON_H_
#define POWER_DAEMON_H_

#include "cros/chromeos_power.h"
#include "power_manager/backlight.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/power_prefs.h"
#include "power_manager/xidle.h"

namespace power_manager {

class Daemon : public XIdleMonitor {
 public:
  Daemon(BacklightController* ctl, PowerPrefs* prefs);

  void Init();
  void Run();
  void OnIdleEvent(bool is_idle, int64 idle_time_ms);
  void SetPlugged(bool plugged);

 private:
  enum PluggedState { kPowerDisconnected, kPowerConnected, kPowerUnknown };

  BacklightController* ctl_;
  XIdle idle_;
  int64 plugged_dim_ms_;
  int64 plugged_off_ms_;
  int64 plugged_suspend_ms_;
  int64 unplugged_dim_ms_;
  int64 unplugged_off_ms_;
  int64 unplugged_suspend_ms_;
  int64 dim_ms_;
  int64 off_ms_;
  int64 suspend_ms_;
  PluggedState plugged_state_;

  static void OnPowerEvent(void* object, const chromeos::PowerStatus& info);
};

}  // namespace power_manager

#endif  // POWER_DAEMON_H_
