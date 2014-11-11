// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/manager.h"

#include <chromeos/dbus/service_constants.h>

namespace apmanager {

Manager::Manager() : org::chromium::apmanager::ManagerAdaptor(this) {}

Manager::~Manager() {}

void Manager::RegisterAsync(
    chromeos::dbus_utils::ExportedObjectManager* object_manager,
    chromeos::dbus_utils::AsyncEventSequencer* sequencer) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_object_.reset(new chromeos::dbus_utils::DBusObject(
      object_manager,
      object_manager ? object_manager->GetBus() : nullptr,
      dbus::ObjectPath(kManagerPath)));
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Manager.RegisterAsync() failed", true));
}

bool Manager::CreateService(chromeos::ErrorPtr* error,
                            dbus::ObjectPath* out_service) {
  return false;
}

bool Manager::RemoveService(chromeos::ErrorPtr* error,
                            const dbus::ObjectPath& in_service) {
  return false;
}

}  // namespace apmanager
