// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DEVICE_DBUS_ADAPTOR_H_
#define WIMAX_MANAGER_DEVICE_DBUS_ADAPTOR_H_

#include <string>

#include "wimax_manager/dbus_adaptor.h"
#include "wimax_manager/dbus_bindings/device.h"

namespace wimax_manager {

class Device;

class DeviceDBusAdaptor : public org::chromium::WiMaxManager::Device_adaptor,
                          public DBusAdaptor {
 public:
  static std::string GetDeviceObjectPath(const Device &device);

  DeviceDBusAdaptor(DBus::Connection *connection, Device *device);
  virtual ~DeviceDBusAdaptor();

  virtual void Enable(DBus::Error &error);  // NOLINT
  virtual void Disable(DBus::Error &error);  // NOLINT
  virtual void Connect(DBus::Error &error);  // NOLINT
  virtual void Disconnect(DBus::Error &error);  // NOLINT

 private:
  Device *device_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDBusAdaptor);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DEVICE_DBUS_ADAPTOR_H_
