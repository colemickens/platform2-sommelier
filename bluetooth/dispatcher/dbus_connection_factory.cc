// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dbus_connection_factory.h"

#include <dbus/bus.h>

namespace bluetooth {

scoped_refptr<dbus::Bus> DBusConnectionFactory::GetNewBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  return new dbus::Bus(options);
}

}  // namespace bluetooth
