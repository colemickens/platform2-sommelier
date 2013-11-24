// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_INPUT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_INPUT_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/system/input_observer.h"

typedef int gboolean;
typedef unsigned int guint;

namespace power_manager {

class Clock;
class DBusSenderInterface;
class PrefsInterface;

namespace system {
class InputInterface;
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

    // Handles the power button being pressed or released.
    virtual void HandlePowerButtonEvent(ButtonState state) = 0;

    // Defers the inactivity timeout in response to VT2 being active (since
    // Chrome can't detect user activity).
    virtual void DeferInactivityTimeoutForVT2() = 0;

    // Shuts the system down in reponse to the power button being pressed while
    // no display is connected.
    virtual void ShutDownForPowerButtonWithNoDisplay() = 0;

    // Handles Chrome failing to acknowledge a power button press quickly
    // enough.
    virtual void HandleMissingPowerButtonAcknowledgment() = 0;
  };

  // Amount of time to wait for Chrome to acknowledge power button presses, in
  // milliseconds.
  static const int kPowerButtonAcknowledgmentTimeoutMs = 2000;

  InputController(system::InputInterface* input,
                  Delegate* delegate,
                  DBusSenderInterface* dbus_sender);
  virtual ~InputController();

  Clock* clock_for_testing() { return clock_.get(); }

  void Init(PrefsInterface* prefs);

  // Calls HandlePowerButtonAcknowledgmentTimeout(). Returns false if
  // |power_button_acknowledgment_timeout_id_| isn't set.
  bool TriggerPowerButtonAcknowledgmentTimeoutForTesting();

  // Calls CheckActiveVT(). Returns false if |check_active_vt_timeout_id_| isn't
  // set or if the timeout is canceled.
  bool TriggerCheckActiveVTTimeoutForTesting();

  // Handles acknowledgment from that a power button press was handled.
  // |timestamp| is the timestamp from the original event.
  void HandlePowerButtonAcknowledgment(const base::TimeTicks& timestamp);

  // system::InputObserver implementation:
  virtual void OnLidEvent(LidState state) OVERRIDE;
  virtual void OnPowerButtonEvent(ButtonState state) OVERRIDE;

 private:
  // Asks |delegate_| to defer the inactivity timeout if the second virtual
  // terminal is currently active (which typically means that the user is doing
  // something on the console in dev mode, so Chrome won't be reporting user
  // activity to keep power management from kicking in).
  SIGNAL_CALLBACK_0(InputController, gboolean, CheckActiveVT);

  // Tells |delegate_| when Chrome hasn't acknowledged a power button press
  // quickly enough.
  SIGNAL_CALLBACK_0(InputController,
                    gboolean,
                    HandlePowerButtonAcknowledgmentTimeout);

  system::InputInterface* input_;  // not owned
  Delegate* delegate_;  // not owned
  DBusSenderInterface* dbus_sender_;  // not owned

  scoped_ptr<Clock> clock_;

  // True if the device doesn't have an internal display.
  bool only_has_external_display_;

  LidState lid_state_;

  // Timestamp from the most recent power-button-down event that Chrome is
  // expected to acknowledge. Unset when the power button isn't pressed or if
  // Chrome has already acknowledged the event.
  base::TimeTicks expected_power_button_acknowledgment_timestamp_;

  // GLib source ID for a timeout that calls
  // HandlePowerButtonAcknowledgmentTimeout().
  guint power_button_acknowledgment_timeout_id_;

  // GLib source ID for a timeout that calls CheckActiveVT() periodically.
  guint check_active_vt_timeout_id_;

  DISALLOW_COPY_AND_ASSIGN(InputController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_INPUT_CONTROLLER_H_
