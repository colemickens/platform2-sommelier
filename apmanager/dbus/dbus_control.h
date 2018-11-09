// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DBUS_DBUS_CONTROL_H_
#define APMANAGER_DBUS_DBUS_CONTROL_H_

#include <memory>

#include <base/macros.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/bus.h>

#include "apmanager/control_interface.h"
#include "apmanager/manager.h"

namespace apmanager {

// D-Bus control interface for IPC through D-Bus.
class DBusControl : public ControlInterface {
 public:
  DBusControl();
  ~DBusControl() override;

  // Inheritted from ControlInterface.
  void Init() override;
  void Shutdown() override;
  std::unique_ptr<ConfigAdaptorInterface> CreateConfigAdaptor(
      Config* config, int service_identifier) override;
  std::unique_ptr<DeviceAdaptorInterface> CreateDeviceAdaptor(
      Device* device) override;
  std::unique_ptr<ManagerAdaptorInterface> CreateManagerAdaptor(
      Manager* manager) override;
  std::unique_ptr<ServiceAdaptorInterface> CreateServiceAdaptor(
      Service* device) override;
  std::unique_ptr<FirewallProxyInterface> CreateFirewallProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;
  std::unique_ptr<ShillProxyInterface> CreateShillProxy(
      const base::Closure& service_appeared_callback,
      const base::Closure& service_vanished_callback) override;

 private:
  // Invoked when D-Bus objects for both ObjectManager and Manager
  // are registered to the bus.
  void OnObjectRegistrationCompleted(bool registration_success);

  // NOTE: No dedicated bus is needed for the proxies, since the proxies
  // being created here doesn't listen for any broadcast signals.
  // Use a dedicated bus for the proxies if this condition is not true
  // anymore.
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<brillo::dbus_utils::ExportedObjectManager> object_manager_;
  std::unique_ptr<Manager> manager_;

  DISALLOW_COPY_AND_ASSIGN(DBusControl);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_DBUS_CONTROL_H_
