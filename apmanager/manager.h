// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MANAGER_H_
#define APMANAGER_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/dbus_service_watcher.h>

#include "apmanager/dbus_adaptors/org.chromium.apmanager.Manager.h"

#include "apmanager/device_info.h"
#include "apmanager/firewall_manager.h"
#include "apmanager/service.h"
#include "apmanager/shill_proxy.h"

namespace apmanager {

class Manager : public org::chromium::apmanager::ManagerAdaptor,
                public org::chromium::apmanager::ManagerInterface {
 public:
  Manager();
  virtual ~Manager();

  // Implementation of ManagerInterface.
  // Handles calls to org.chromium.apmanager.Manager.CreateService().
  // This is an asynchronous call, response is invoked when Service and Config
  // dbus objects complete the DBus service registration.
  virtual void CreateService(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse<dbus::ObjectPath>>
          response,
      dbus::Message* message);
  // Handles calls to org.chromium.apmanager.Manager.RemoveService().
  virtual bool RemoveService(chromeos::ErrorPtr* error,
                             dbus::Message* message,
                             const dbus::ObjectPath& in_service);

  // Register DBus object.
  void RegisterAsync(
      chromeos::dbus_utils::ExportedObjectManager* object_manager,
      const scoped_refptr<dbus::Bus>& bus,
      chromeos::dbus_utils::AsyncEventSequencer* sequencer);

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
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse<dbus::ObjectPath>>
          response,
      scoped_ptr<Service> service,
      bool success);

  // A callback that will be called when a Device D-Bus object/interface is
  // exported successfully and ready to be used.
  void OnDeviceRegistered(scoped_refptr<Device> device, bool success);

  // This is invoked when the owner of an AP service disappeared.
  void OnAPServiceOwnerDisappeared(int service_identifier);

  int service_identifier_;
  int device_identifier_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  scoped_refptr<dbus::Bus> bus_;
  std::vector<std::unique_ptr<Service>> services_;
  std::vector<scoped_refptr<Device>> devices_;
  // DBus service watchers for the owner of AP services.
  using DBusServiceWatcher = chromeos::dbus_utils::DBusServiceWatcher;
  std::map<int, std::unique_ptr<DBusServiceWatcher>> service_watchers_;
  DeviceInfo device_info_;

  // Proxy to shill DBus services.
  ShillProxy shill_proxy_;
  // Proxy to DBus service for managing firewall rules.
  FirewallManager firewall_manager_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace apmanager

#endif  // APMANAGER_MANAGER_H_
