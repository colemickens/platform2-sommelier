// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service.h"

#include <string>

#include <chromeos/dbus/service_constants.h>

namespace dlcservice {

// kDlcServiceName is defined in chromeos/dbus/service_constant.h
DlcService::DlcService() : DBusServiceDaemon(kDlcServiceName) {}

void DlcService::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
      nullptr, bus_,
      org::chromium::DlcServiceInterfaceAdaptor::GetObjectPath());
  dbus_adaptor_ = std::make_unique<dlcservice::DlcServiceDBusAdaptor>();
  dbus_adaptor_->RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("RegisterAsync() failed.", true));
}

}  // namespace dlcservice
