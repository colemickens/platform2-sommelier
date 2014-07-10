// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DISPLAY_DISPLAY_POWER_SETTER_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_DISPLAY_DISPLAY_POWER_SETTER_STUB_H_

#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>

#include "power_manager/powerd/system/display/display_power_setter.h"

namespace power_manager {
namespace system {

// Stub DisplayPowerSetterInterface implementation for tests that just
// keeps track of the most-recently-requested change.
class DisplayPowerSetterStub : public DisplayPowerSetterInterface {
 public:
  DisplayPowerSetterStub();
  virtual ~DisplayPowerSetterStub();

  chromeos::DisplayPowerState state() const { return state_; }
  base::TimeDelta delay() const { return delay_; }
  int num_power_calls() const { return num_power_calls_; }
  void reset_num_power_calls() { num_power_calls_ = 0; }
  bool dimmed() const { return dimmed_; }

  // DisplayPowerSetterInterface implementation:
  virtual void SetDisplayPower(chromeos::DisplayPowerState state,
                               base::TimeDelta delay) OVERRIDE;
  virtual void SetDisplaySoftwareDimming(bool dimmed) OVERRIDE;

 private:
  // Arguments passed to most-recent SetDisplayPower() call.
  chromeos::DisplayPowerState state_;
  base::TimeDelta delay_;

  // Number of times that SetDisplayPower() has been called.
  int num_power_calls_;

  // Value of most-recent SetDisplaySoftwareDimming() call.
  bool dimmed_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPowerSetterStub);
};

}  // system
}  // power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DISPLAY_DISPLAY_POWER_SETTER_STUB_H_
