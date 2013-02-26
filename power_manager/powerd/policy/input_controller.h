// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_INPUT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_INPUT_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/input_observer.h"

namespace power_manager {

class DBusSenderInterface;
class PrefsInterface;

namespace system {
class Input;
}  // namespace system

namespace policy {

// InputController responds to input events (e.g. lid open/close, power button,
// etc.).
class InputController : public system::InputObserver {
 public:
  // Interface for delegates responsible for performing actions on behalf of
  // InputController.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Handles the lid being closed.
    virtual void HandleLidClosed() = 0;

    // Handles the lid being opened.
    virtual void HandleLidOpened() = 0;

    // Ensures that the backlight is turned on.
    virtual void EnsureBacklightIsOn() = 0;

    // Sends metrics in response to the power button being pressed or released.
    virtual void SendPowerButtonMetric(bool down,
                                       base::TimeTicks timestamp) = 0;
  };

  InputController(system::Input* input,
                  Delegate* delegate,
                  DBusSenderInterface* dbus_sender,
                  const base::FilePath& run_dir);
  ~InputController();

  void Init(PrefsInterface* prefs);

  // system::InputObserver implementation:
  virtual void OnLidEvent(LidState state) OVERRIDE;
  virtual void OnPowerButtonEvent(ButtonState state) OVERRIDE;

 private:
  system::Input* input_;  // not owned
  Delegate* delegate_;  // not owned
  DBusSenderInterface* dbus_sender_;  // not owned

  LidState lid_state_;

  // Should |input_| be queried for the state of the lid?
  bool use_input_for_lid_;

  DISALLOW_COPY_AND_ASSIGN(InputController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_INPUT_CONTROLLER_H_
