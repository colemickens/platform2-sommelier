// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERMAN_H_
#define POWER_MANAGER_POWERMAN_H_

#include <dbus/dbus-glib-lowlevel.h>
#include <gdk/gdk.h>

#include "power_manager/input.h"
#include "power_manager/power_prefs.h"

namespace power_manager {

class PowerManDaemon {
 public:
  PowerManDaemon(bool use_input_for_lid, bool use_input_for_key_power,
                 PowerPrefs* prefs);

  void Init();
  void Run();

 private:
  enum LidState { kLidClosed, kLidOpened };
  inline LidState GetLidState(int value) {
    // value == 0 is open. value == 1 is closed.
    return (value == 1) ? kLidClosed : kLidOpened;
  }

  enum PowerButtonState { kPowerButtonDown, kPowerButtonUp };
  inline PowerButtonState GetPowerButtonState(int value) {
    // value == 0 is button up. value == 1 is button down.
    // value == 2 is key repeat.
    return (value > 0) ? kPowerButtonDown : kPowerButtonUp;
  }


  // Handler for input events. |object| contains a pointer to a PowerManDaemon
  // object. |type| contains the event type (lid or power button). |value|
  // contains the new state of this input device.
  static void OnInputEvent(void* object, InputType type, int value);

  // Standard handler for dbus messages. |data| contains a pointer to a
  // PowerManDaemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Callback for timeout event started when input event signals suspend.
  // |object| contains a pointer to a PowerManDaemon object.
  static gboolean RetrySuspend(void* object);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Shutdown and suspend the system.
  void Shutdown();
  void Suspend();

  GMainLoop* loop_;
  Input input_;
  bool use_input_for_lid_;
  bool use_input_for_key_power_;
  PowerPrefs* prefs_;
  LidState lidstate_;
  int power_button_state_;
  int64 retry_suspend_ms_;
  int64 retry_suspend_attempts_;
  int retry_suspend_count_;
  guint retry_suspend_event_id_;
};

} // namespace power_manager

#endif // POWER_MANAGER_POWERMAN_H_
