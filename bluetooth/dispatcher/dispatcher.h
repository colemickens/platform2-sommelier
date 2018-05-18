// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DISPATCHER_H_
#define BLUETOOTH_DISPATCHER_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include <brillo/dbus/exported_object_manager.h>
#include <dbus/object_manager.h>

#include "bluetooth/dispatcher/impersonation_object_manager_interface.h"

namespace bluetooth {

// Exports BlueZ-compatible API and dispatches the requests to BlueZ or newblue.
class Dispatcher {
 public:
  explicit Dispatcher(scoped_refptr<dbus::Bus> bus);
  ~Dispatcher();

  // Initializes the daemon D-Bus operations.
  bool Init();

  // Frees up all resources, stopping all D-Bus operations.
  // Currently only needed in test.
  void Shutdown();

 private:
  scoped_refptr<dbus::Bus> bus_;

  // The exported ObjectManager interface which is the impersonation of BlueZ's
  // ObjectManager.
  std::unique_ptr<ExportedObjectManagerWrapper>
      exported_object_manager_wrapper_;

  // Connects to BlueZ's object manager. Owned by |bus_|.
  dbus::ObjectManager* bluez_object_manager_;

  // Impersonates BlueZ's objects on various interfaces.
  std::map<std::string, std::unique_ptr<ImpersonationObjectManagerInterface>>
      impersonation_object_manager_interfaces_;

  std::unique_ptr<DBusConnectionFactory> dbus_connection_factory_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DISPATCHER_H_
