// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hammerd/dbus_wrapper.h"

#include <chromeos/dbus/service_constants.h>

namespace hammerd {

DBusWrapper::DBusWrapper() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect()) << "Failed to connect to system bus.";
  CHECK(bus_->RequestOwnershipAndBlock(kHammerdServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Failed to request ownership.";
  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kHammerdServicePath));
}

void DBusWrapper::SendSignal(dbus::Signal* signal) {
  DCHECK(signal);
  exported_object_->SendSignal(signal);
}

void DBusWrapper::SendSignal(const std::string& signal_name) {
  LOG(INFO) << "Send the DBus signal: " << signal_name;
  dbus::Signal signal(kHammerdInterface, signal_name);
  SendSignal(&signal);
}

}  // namespace hammerd
