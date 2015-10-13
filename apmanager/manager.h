// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MANAGER_H_
#define APMANAGER_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <brillo/dbus/dbus_service_watcher.h>

#include "apmanager/device_info.h"
#include "apmanager/firewall_manager.h"
#include "apmanager/service.h"
#include "apmanager/shill_manager.h"
#include "dbus_bindings/org.chromium.apmanager.Manager.h"

namespace apmanager {

class Manager : public org::chromium::apmanager::ManagerAdaptor,
                public org::chromium::apmanager::ManagerInterface {
 public:
  template<typename T>
  using DBusMethodResponse = brillo::dbus_utils::DBusMethodResponse<T>;

  Manager();
  virtual ~Manager();

  // Implementation of ManagerInterface.
  // Handles calls to org.chromium.apmanager.Manager.CreateService().
  // This is an asynchronous call, response is invoked when Service and Config
  // dbus objects complete the DBus service registration.
  virtual void CreateService(
      std::unique_ptr<DBusMethodResponse<dbus::ObjectPath>> response,
      dbus::Message* message);
  // Handles calls to org.chromium.apmanager.Manager.RemoveService().
  virtual bool RemoveService(brillo::ErrorPtr* error,
                             dbus::Message* message,
                             const dbus::ObjectPath& in_service);

  // Register DBus object.
  void RegisterAsync(
      brillo::dbus_utils::ExportedObjectManager* object_manager,
      const scoped_refptr<dbus::Bus>& bus,
      brillo::dbus_utils::AsyncEventSequencer* sequencer);

  virtual void Start();
  virtual void Stop();

  virtual void RegisterDevice(scoped_refptr<Device> device);

  // Return an unuse device with AP interface mode support.
  virtual scoped_refptr<Device> GetAvailableDevice();

  // Return the device that's associated with the given interface
  // |interface_name|.
  virtual scoped_refptr<Device> GetDeviceFromInterfaceName(
      const std::string& interface_name);

  // Claim the given interface |interface_name| from shill.
  virtual void ClaimInterface(const std::string& interface_name);
  // Release the given interface |interface_name| to shill.
  virtual void ReleaseInterface(const std::string& interface_name);

  // Request/release access to DHCP port for the specified interface.
  virtual void RequestDHCPPortAccess(const std::string& interface);
  virtual void ReleaseDHCPPortAccess(const std::string& interface);

 private:
  friend class ManagerTest;

  // A callback that will be called when the Service/Config D-Bus
  // objects/interfaces are exported successfully and ready to be used.
  void OnServiceRegistered(
      std::unique_ptr<DBusMethodResponse<dbus::ObjectPath>> response,
      std::unique_ptr<Service> service,
      bool success);

  // A callback that will be called when a Device D-Bus object/interface is
  // exported successfully and ready to be used.
  void OnDeviceRegistered(scoped_refptr<Device> device, bool success);

  // This is invoked when the owner of an AP service disappeared.
  void OnAPServiceOwnerDisappeared(int service_identifier);

  int service_identifier_;
  int device_identifier_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  scoped_refptr<dbus::Bus> bus_;
  std::vector<std::unique_ptr<Service>> services_;
  std::vector<scoped_refptr<Device>> devices_;
  // DBus service watchers for the owner of AP services.
  using DBusServiceWatcher = brillo::dbus_utils::DBusServiceWatcher;
  std::map<int, std::unique_ptr<DBusServiceWatcher>> service_watchers_;
  DeviceInfo device_info_;

  // Manager for communicating with shill (connection manager).
  ShillManager shill_manager_;
  // Manager for communicating with remote firewall service.
  FirewallManager firewall_manager_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace apmanager

#endif  // APMANAGER_MANAGER_H_
