// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERMAN_H_
#define POWER_MANAGER_POWERMAN_H_

#include <dbus/dbus-glib-lowlevel.h>
#include <gdk/gdk.h>

#include "power_manager/input.h"

namespace power_manager {

class PowerManDaemon {
 public:
  PowerManDaemon(bool use_input_for_lid, bool use_input_for_key_power);

  void Init();
  void Run();

 private:
  // Handler for input events. |object| contains a pointer to a PowerManDaemon
  // object. |type| contains the event type (lid or power button). |value|
  // contains the new state of this input device.
  static void OnInputEvent(void* object, InputType type, int value);

  // Standard handler for dbus messages. |data| contains a pointer to a
  // PowerManDaemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Shutdown and suspend the system.
  void Shutdown();
  void Suspend();

  GMainLoop* loop_;
  Input input_;
  bool use_input_for_lid_;
  bool use_input_for_key_power_;
  bool lidstate_;
  int power_button_state_;
};

} // namespace power_manager

#endif // POWER_MANAGER_POWERMAN_H_
