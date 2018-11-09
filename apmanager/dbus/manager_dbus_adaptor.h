// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DBUS_MANAGER_DBUS_ADAPTOR_H_
#define APMANAGER_DBUS_MANAGER_DBUS_ADAPTOR_H_

#include <map>
#include <memory>
#include <utility>

#include <base/macros.h>
#include <brillo/dbus/dbus_service_watcher.h>
#include <dbus_bindings/org.chromium.apmanager.Manager.h>

#include "apmanager/manager_adaptor_interface.h"

namespace apmanager {

class Manager;
class Service;

class ManagerDBusAdaptor : public org::chromium::apmanager::ManagerInterface,
                           public ManagerAdaptorInterface {
 public:
  ManagerDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                     brillo::dbus_utils::ExportedObjectManager* object_manager,
                     Manager* manager);
  ~ManagerDBusAdaptor() override;

  // Implementation of org::chromium::apmanager::ManagerInterface.
  bool CreateService(brillo::ErrorPtr* dbus_error,
                     dbus::Message* message,
                     dbus::ObjectPath* out_service) override;
  bool RemoveService(brillo::ErrorPtr* dbus_error,
                     dbus::Message* message,
                     const dbus::ObjectPath& in_service) override;

  // Implementation of ManagerAdaptorInterface.
  void RegisterAsync(
      const base::Callback<void(bool)>& completion_callback) override;

 private:
  using DBusServiceWatcher = brillo::dbus_utils::DBusServiceWatcher;
  // Context for service owner watcher.
  struct ServiceOwnerWatcherContext {
    ServiceOwnerWatcherContext() {}
    ServiceOwnerWatcherContext(const scoped_refptr<Service>& in_service,
                               std::unique_ptr<DBusServiceWatcher> in_watcher)
        : service(in_service),
          watcher(std::move(in_watcher)) {}
    scoped_refptr<Service> service;
    std::unique_ptr<DBusServiceWatcher> watcher;
  };

  // Invoked when the owner of a Service vanished.
  void OnServiceOwnerVanished(const dbus::ObjectPath& service_path);

  org::chromium::apmanager::ManagerAdaptor adaptor_;
  brillo::dbus_utils::DBusObject dbus_object_;
  scoped_refptr<dbus::Bus> bus_;
  Manager* manager_;
  // Map of service path to owner monitor context.
  std::map<dbus::ObjectPath, ServiceOwnerWatcherContext>
      service_owner_watchers_;

  DISALLOW_COPY_AND_ASSIGN(ManagerDBusAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_MANAGER_DBUS_ADAPTOR_H_
