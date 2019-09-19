// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_MOCK_DBUS_CONNECTION_FACTORY_H_
#define BLUETOOTH_DISPATCHER_MOCK_DBUS_CONNECTION_FACTORY_H_

#include <dbus/bus.h>
#include <gmock/gmock.h>

#include "bluetooth/dispatcher/dbus_connection_factory.h"

namespace bluetooth {

class MockDBusConnectionFactory : public DBusConnectionFactory {
 public:
  using DBusConnectionFactory::DBusConnectionFactory;

  MOCK_METHOD(scoped_refptr<dbus::Bus>, GetNewBus, (), (override));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_MOCK_DBUS_CONNECTION_FACTORY_H_
