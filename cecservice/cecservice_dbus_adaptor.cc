// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cecservice/cecservice_dbus_adaptor.h"

#include <chromeos/dbus/service_constants.h>
#include <dbus/object_path.h>

namespace cecservice {

CecServiceDBusAdaptor::CecServiceDBusAdaptor(scoped_refptr<dbus::Bus> bus)
    : org::chromium::CecServiceAdaptor(this),
      dbus_object_(nullptr, bus, dbus::ObjectPath(kCecServicePath)) {}

CecServiceDBusAdaptor::~CecServiceDBusAdaptor() = default;

void CecServiceDBusAdaptor::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(cb);
}

bool CecServiceDBusAdaptor::SendStandByToAllDevices(brillo::ErrorPtr* error) {
  return true;
}

bool CecServiceDBusAdaptor::SendWakeUpToAllDevices(brillo::ErrorPtr* error) {
  return true;
}

}  // namespace cecservice
