// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DBUS_CONNECTION_FACTORY_H_
#define BLUETOOTH_DISPATCHER_DBUS_CONNECTION_FACTORY_H_

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

namespace bluetooth {

// A factory to produce a D-Bus system bus connection for forwarding.
// Useful to be mocked to produce mock dbus::Bus for testing.
class DBusConnectionFactory {
 public:
  DBusConnectionFactory() = default;
  virtual ~DBusConnectionFactory() = default;

  // Returns a new dbus::Bus connection to be used for D-Bus forwarding.
  virtual scoped_refptr<dbus::Bus> GetNewBus();

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusConnectionFactory);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DBUS_CONNECTION_FACTORY_H_
