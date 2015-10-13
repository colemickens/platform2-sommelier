// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/manager.h"

#include <base/bind.h>

#if !defined(__ANDROID__)
#include <chromeos/dbus/service_constants.h>
#else
#include "dbus/apmanager/dbus-constants.h"
#endif  // __ANDROID__

using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::ExportedObjectManager;
using brillo::dbus_utils::DBusMethodResponse;
using std::string;

namespace apmanager {

Manager::Manager()
    : org::chromium::apmanager::ManagerAdaptor(this),
      service_identifier_(0),
      device_identifier_(0),
      device_info_(this) {}

Manager::~Manager() {
  // Terminate all services before cleanup other resources.
  for (auto& service : services_) {
    service.reset();
  }
}

void Manager::RegisterAsync(ExportedObjectManager* object_manager,
                            const scoped_refptr<dbus::Bus>& bus,
                            AsyncEventSequencer* sequencer) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_object_.reset(
      new brillo::dbus_utils::DBusObject(
          object_manager,
          bus,
          org::chromium::apmanager::ManagerAdaptor::GetObjectPath()));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
  bus_ = bus;

  shill_manager_.Init(bus);
  firewall_manager_.Init(bus);
}

void Manager::Start() {
  device_info_.Start();
}

void Manager::Stop() {
  device_info_.Stop();
}

void Manager::CreateService(
    std::unique_ptr<DBusMethodResponse<dbus::ObjectPath>> response,
    dbus::Message* message) {
  LOG(INFO) << "Manager::CreateService";
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  std::unique_ptr<Service> service(new Service(this, service_identifier_));

  service->RegisterAsync(
      dbus_object_->GetObjectManager().get(), bus_, sequencer.get());
  sequencer->OnAllTasksCompletedCall({
      base::Bind(&Manager::OnServiceRegistered,
                 base::Unretained(this),
                 base::Passed(&response),
                 base::Passed(&service))
  });

  base::Closure on_connection_vanish = base::Bind(
      &Manager::OnAPServiceOwnerDisappeared,
      base::Unretained(this),
      service_identifier_);
  service_watchers_[service_identifier_].reset(
      new DBusServiceWatcher{bus_, message->GetSender(), on_connection_vanish});
  service_identifier_++;
}

bool Manager::RemoveService(brillo::ErrorPtr* error,
                            dbus::Message* message,
                            const dbus::ObjectPath& in_service) {
  for (auto it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->dbus_path() == in_service) {
      // Verify the owner.
      auto watcher = service_watchers_.find((*it)->identifier());
      CHECK(watcher != service_watchers_.end())
          << "DBus watcher not created for service: " << (*it)->identifier();
      if (watcher->second->connection_name() != message->GetSender()) {
        brillo::Error::AddToPrintf(
            error, FROM_HERE, brillo::errors::dbus::kDomain, kManagerError,
            "Service %d is owned by another local process.",
            (*it)->identifier());
        return false;
      }
      service_watchers_.erase(watcher);

      services_.erase(it);
      return true;
    }
  }

  brillo::Error::AddTo(
      error, FROM_HERE, brillo::errors::dbus::kDomain, kManagerError,
      "Service does not exist");
  return false;
}

scoped_refptr<Device> Manager::GetAvailableDevice() {
  for (const auto& device : devices_) {
    // Look for an unused device with AP interface mode support.
    if (!device->GetInUse() && !device->GetPreferredApInterface().empty()) {
      return device;
    }
  }
  return nullptr;
}

scoped_refptr<Device> Manager::GetDeviceFromInterfaceName(
    const string& interface_name) {
  for (const auto& device : devices_) {
    if (device->InterfaceExists(interface_name)) {
      return device;
    }
  }
  return nullptr;
}

void Manager::RegisterDevice(scoped_refptr<Device> device) {
  LOG(INFO) << "Manager::RegisterDevice: registering device "
            << device->GetDeviceName();
  // Register device DBbus interfaces.
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  device->RegisterAsync(dbus_object_->GetObjectManager().get(),
                        bus_,
                        sequencer.get(),
                        device_identifier_++);
  sequencer->OnAllTasksCompletedCall({
    base::Bind(&Manager::OnDeviceRegistered,
               base::Unretained(this),
               device)
  });
}

void Manager::ClaimInterface(const string& interface_name) {
  shill_manager_.ClaimInterface(interface_name);
}

void Manager::ReleaseInterface(const string& interface_name) {
  shill_manager_.ReleaseInterface(interface_name);
}

void Manager::RequestDHCPPortAccess(const string& interface) {
  firewall_manager_.RequestDHCPPortAccess(interface);
}

void Manager::ReleaseDHCPPortAccess(const string& interface) {
  firewall_manager_.ReleaseDHCPPortAccess(interface);
}

void Manager::OnServiceRegistered(
    std::unique_ptr<DBusMethodResponse<dbus::ObjectPath>> response,
    std::unique_ptr<Service> service,
    bool success) {
  LOG(INFO) << "ServiceRegistered";
  // Success should always be true since we've said that failures are fatal.
  CHECK(success) << "Init of one or more objects has failed.";

  // Remove this service if the owner doesn't exist anymore. It is theoretically
  // possible to have the owner disappear before the AP service complete its
  // registration with DBus.
  if (service_watchers_.find(service->identifier()) ==
      service_watchers_.end()) {
    LOG(INFO) << "Service " << service->identifier()
              << ": owner doesn't exist anymore";
    service.reset();
    return;
  }

  // Add service to the service list and return the service dbus path for the
  // CreateService call.
  dbus::ObjectPath service_path = service->dbus_path();
  services_.push_back(std::move(service));
  response->Return(service_path);
}

void Manager::OnDeviceRegistered(scoped_refptr<Device> device, bool success) {
  // Success should always be true since we've said that failures are fatal.
  CHECK(success) << "Init of one or more objects has failed.";

  devices_.push_back(device);
  // TODO(zqiu): Property update for available devices.
}

void Manager::OnAPServiceOwnerDisappeared(int service_identifier) {
  LOG(INFO) << "Owner for service " << service_identifier << " disappeared";
  // Remove service watcher.
  auto watcher = service_watchers_.find(service_identifier);
  CHECK(watcher != service_watchers_.end())
      << "Owner disappeared without watcher setup";
  service_watchers_.erase(watcher);

  // Remove the service.
  for (auto it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->identifier() == service_identifier) {
      services_.erase(it);
      return;
    }
  }
  LOG(INFO) << "Owner for service " << service_identifier
            << " disappeared before it is registered";
}

}  // namespace apmanager
