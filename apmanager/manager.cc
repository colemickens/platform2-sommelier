// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/manager.h"

#include <chromeos/dbus/service_constants.h>

namespace apmanager {

Manager::Manager() {}

Manager::~Manager() {}

void Manager::InitDBus(
    chromeos::dbus_utils::ExportedObjectManager* object_manager) {
  dbus_adaptor_.reset(
     new org::chromium::apmanager::ManagerAdaptor(
         object_manager, kManagerPath, this));
}

dbus::ObjectPath Manager::CreateService(chromeos::ErrorPtr* error) {
  return dbus::ObjectPath();
}

void Manager::RemoveService(chromeos::ErrorPtr* error,
                            const dbus::ObjectPath& service) {
}

}  // namespace apmanager
