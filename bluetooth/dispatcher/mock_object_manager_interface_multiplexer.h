// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_MOCK_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_
#define BLUETOOTH_DISPATCHER_MOCK_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_

#include <string>

#include <gmock/gmock.h>

#include "bluetooth/dispatcher/object_manager_interface_multiplexer.h"

namespace bluetooth {

class MockObjectManagerInterfaceMultiplexer
    : public ObjectManagerInterfaceMultiplexer {
 public:
  using ObjectManagerInterfaceMultiplexer::ObjectManagerInterfaceMultiplexer;

  MOCK_METHOD(dbus::PropertySet*,
              CreateProperties,
              (const std::string&,
               dbus::ObjectProxy*,
               const dbus::ObjectPath&,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              ObjectAdded,
              (const std::string&, const dbus::ObjectPath&, const std::string&),
              (override));
  MOCK_METHOD(void,
              ObjectRemoved,
              (const std::string&, const dbus::ObjectPath&, const std::string&),
              (override));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_MOCK_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_
