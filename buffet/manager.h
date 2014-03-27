// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_MANAGER_H_
#define BUFFET_MANAGER_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <dbus/message.h>

namespace buffet {

class DBusManager;

// The Manager is responsible for global state of Buffet.  It exposes
// interfaces which affect the entire device such as device registration and
// device state.
class Manager {
 public:
  Manager(DBusManager* dbus_manager);
  ~Manager();

 private:
  // Handles calls to org.chromium.Buffet.Manager.RegisterDevice().
  scoped_ptr<dbus::Response> HandleRegisterDevice(
      dbus::MethodCall* method_call);
  // Handles calls to org.chromium.Buffet.Manager.UpdateState().
  scoped_ptr<dbus::Response> HandleUpdateState(
      dbus::MethodCall* method_call);

  DBusManager* dbus_manager_;  // Weak;  DBusManager should outlive Manager.

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace buffet

#endif  // BUFFET_MANAGER_H_
