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

  MOCK_METHOD4(CreateProperties,
               dbus::PropertySet*(const std::string& service_name,
                                  dbus::ObjectProxy* object_proxy,
                                  const dbus::ObjectPath& object_path,
                                  const std::string& interface_name));
  MOCK_METHOD3(ObjectAdded,
               void(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const std::string& interface_name));
  MOCK_METHOD3(ObjectRemoved,
               void(const std::string& service_name,
                    const dbus::ObjectPath& object_path,
                    const std::string& interface_name));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_MOCK_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_
