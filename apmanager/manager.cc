// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/manager.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using chromeos::dbus_utils::DBusMethodResponse;

namespace apmanager {

Manager::Manager()
    : org::chromium::apmanager::ManagerAdaptor(this),
      service_identifier_(0) {}

Manager::~Manager() {}

void Manager::RegisterAsync(ExportedObjectManager* object_manager,
                            AsyncEventSequencer* sequencer) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_object_.reset(
      new chromeos::dbus_utils::DBusObject(
          object_manager,
          object_manager ? object_manager->GetBus() : nullptr,
          dbus::ObjectPath(kManagerPath)));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
}

void Manager::CreateService(
    scoped_ptr<DBusMethodResponse<dbus::ObjectPath>> response) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  scoped_ptr<Service> service(new Service(service_identifier_++));

  service->RegisterAsync(dbus_object_->GetObjectManager().get(), sequencer);
  sequencer->OnAllTasksCompletedCall({
      base::Bind(&Manager::ServiceRegistered,
                 base::Unretained(this),
                 base::Passed(&response),
                 base::Passed(&service))
  });
}

bool Manager::RemoveService(chromeos::ErrorPtr* error,
                            const dbus::ObjectPath& in_service) {
  for (auto it = services_.begin(); it != services_.end(); ++it) {
    if ((*it)->dbus_path() == in_service) {
      services_.erase(it);
      return true;
    }
  }

  chromeos::Error::AddTo(
      error, FROM_HERE, chromeos::errors::dbus::kDomain, kManagerError,
      "Service does not exist");
  return false;
}

void Manager::ServiceRegistered(
    scoped_ptr<DBusMethodResponse<dbus::ObjectPath>> response,
    scoped_ptr<Service> service,
    bool success) {
  // Success should always be true since we've said that failures are fatal.
  CHECK(success) << "Init of one or more objects has failed.";

  // Add service to the service list and return the service dbus path for the
  // CreateService call.
  dbus::ObjectPath service_path = service->dbus_path();
  services_.push_back(std::unique_ptr<Service>(service.release()));
  response->Return(service_path);
}

}  // namespace apmanager
