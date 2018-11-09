// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DEVICE_DBUS_ADAPTOR_H_
#define APMANAGER_DEVICE_DBUS_ADAPTOR_H_

#include <base/macros.h>

#include <dbus_bindings/org.chromium.apmanager.Device.h>

#include "apmanager/device_adaptor_interface.h"

namespace apmanager {

class Device;

class DeviceDBusAdaptor : public org::chromium::apmanager::DeviceInterface,
                          public DeviceAdaptorInterface {
 public:
  DeviceDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                    brillo::dbus_utils::ExportedObjectManager* object_manager,
                    Device* device);
  ~DeviceDBusAdaptor() override;

  // Implementation of DeviceAdaptorInterface.
  void SetDeviceName(const std::string& device_name) override;
  std::string GetDeviceName() override;
  void SetInUse(bool in_use) override;
  bool GetInUse() override;
  void SetPreferredApInterface(const std::string& interface_name) override;
  std::string GetPreferredApInterface() override;

 private:
  org::chromium::apmanager::DeviceAdaptor adaptor_;
  dbus::ObjectPath object_path_;
  brillo::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDBusAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_DEVICE_DBUS_ADAPTOR_H_
