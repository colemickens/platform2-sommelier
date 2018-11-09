// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/dbus/manager_dbus_adaptor.h"

#include <chromeos/dbus/service_constants.h>

#include "apmanager/manager.h"

using brillo::dbus_utils::ExportedObjectManager;
using org::chromium::apmanager::ManagerAdaptor;

namespace apmanager {

ManagerDBusAdaptor::ManagerDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    ExportedObjectManager* object_manager,
    Manager* manager)
    : adaptor_(this),
      dbus_object_(object_manager, bus, ManagerAdaptor::GetObjectPath()),
      bus_(bus),
      manager_(manager) {}

ManagerDBusAdaptor::~ManagerDBusAdaptor() {}

void ManagerDBusAdaptor::RegisterAsync(
    const base::Callback<void(bool)>& completion_callback) {
  adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(completion_callback);
}

bool ManagerDBusAdaptor::CreateService(brillo::ErrorPtr* dbus_error,
                                       dbus::Message* message,
                                       dbus::ObjectPath* out_service) {
  auto service = manager_->CreateService();
  if (!service) {
    brillo::Error::AddTo(dbus_error,
                         FROM_HERE,
                         brillo::errors::dbus::kDomain,
                         kErrorInternalError,
                         "Failed to create new service");
    return false;
  }

  *out_service = service->adaptor()->GetRpcObjectIdentifier();

  // Setup monitoring for the service's remote owner.
  service_owner_watchers_[*out_service] =
      ServiceOwnerWatcherContext(
          service,
          std::unique_ptr<DBusServiceWatcher>(
              new DBusServiceWatcher(
                  bus_,
                  message->GetSender(),
                  base::Bind(&ManagerDBusAdaptor::OnServiceOwnerVanished,
                             base::Unretained(this),
                             *out_service))));
  return true;
}

bool ManagerDBusAdaptor::RemoveService(brillo::ErrorPtr* dbus_error,
                                       dbus::Message* message,
                                       const dbus::ObjectPath& in_service) {
  auto watcher_context = service_owner_watchers_.find(in_service);
  if (watcher_context == service_owner_watchers_.end()) {
    brillo::Error::AddToPrintf(
        dbus_error,
        FROM_HERE,
        brillo::errors::dbus::kDomain,
        kErrorInvalidArguments,
        "Service %s not found",
        in_service.value().c_str());
    return false;
  }

  Error error;
  manager_->RemoveService(watcher_context->second.service, &error);
  service_owner_watchers_.erase(watcher_context);
  return !error.ToDBusError(dbus_error);
}

void ManagerDBusAdaptor::OnServiceOwnerVanished(
    const dbus::ObjectPath& service_path) {
  LOG(INFO) << "Owner for service " << service_path.value() << " vanished";
  // Remove service watcher.
  auto watcher_context = service_owner_watchers_.find(service_path);
  CHECK(watcher_context != service_owner_watchers_.end())
      << "Owner vanished without watcher setup.";

  // Tell Manager to remove this service.
  manager_->RemoveService(watcher_context->second.service, nullptr);
  service_owner_watchers_.erase(watcher_context);
}

}  // namespace apmanager
