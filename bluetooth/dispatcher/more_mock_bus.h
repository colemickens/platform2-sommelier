// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_MORE_MOCK_BUS_H_
#define BLUETOOTH_DISPATCHER_MORE_MOCK_BUS_H_

#include <dbus/dbus.h>

#include "dbus/mock_bus.h"

namespace bluetooth {

// Like MockBus, but with more mocking.
class MoreMockBus : public dbus::MockBus {
 public:
  using MockBus::MockBus;
  MOCK_METHOD2(AddFilterFunction, void(DBusHandleMessageFunction, void*));
  MOCK_METHOD2(RemoveFilterFunction, void(DBusHandleMessageFunction, void*));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_MORE_MOCK_BUS_H_
