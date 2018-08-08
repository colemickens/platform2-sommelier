// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_BLUEZ_INTERFACE_HANDLER_H_
#define BLUETOOTH_DISPATCHER_BLUEZ_INTERFACE_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "bluetooth/dispatcher/impersonation_object_manager_interface.h"

namespace bluetooth {

class BluezInterfaceHandler : public InterfaceHandler {
 public:
  BluezInterfaceHandler() = default;

  const PropertyFactoryMap& GetPropertyFactoryMap() const override {
    return property_factory_map_;
  }

  const std::set<std::string>& GetMethodNames() const override {
    return method_names_;
  }

 protected:
  template <typename T>
  void AddPropertyFactory(const std::string& property_name) {
    property_factory_map_.emplace(property_name,
                                  std::make_unique<PropertyFactory<T>>());
  }

  void AddMethodName(const std::string& method_name) {
    method_names_.insert(method_name);
  }

 private:
  InterfaceHandler::PropertyFactoryMap property_factory_map_;
  std::set<std::string> method_names_;

  DISALLOW_COPY_AND_ASSIGN(BluezInterfaceHandler);
};

// org.bluez.Adapter1 interface.
class BluezAdapterInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezAdapterInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezAdapterInterfaceHandler);
};

// org.bluez.Device1 interface.
class BluezDeviceInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezDeviceInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezDeviceInterfaceHandler);
};

// org.bluez.GattCharacteristic1 interface.
class BluezGattCharacteristicInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezGattCharacteristicInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezGattCharacteristicInterfaceHandler);
};

// org.bluez.Input1 interface.
class BluezInputInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezInputInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezInputInterfaceHandler);
};

// org.bluez.Media1 interface.
class BluezMediaInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezMediaInterfaceHandler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezMediaInterfaceHandler);
};

// org.bluez.GattService1 interface.
class BluezGattServiceInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezGattServiceInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezGattServiceInterfaceHandler);
};

// org.bluez.LEAdvertisingManager1 interface.
class BluezLeAdvertisingManagerInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezLeAdvertisingManagerInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezLeAdvertisingManagerInterfaceHandler);
};

// org.bluez.GattDescriptor1 interface.
class BluezGattDescriptorInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezGattDescriptorInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezGattDescriptorInterfaceHandler);
};

// org.bluez.MediaTransport1 interface.
class BluezMediaTransportInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezMediaTransportInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezMediaTransportInterfaceHandler);
};

// org.bluez.AgentManager1 interface.
class BluezAgentManagerInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezAgentManagerInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezAgentManagerInterfaceHandler);
};

// org.bluez.ProfileManager1 interface.
class BluezProfileManagerInterfaceHandler : public BluezInterfaceHandler {
 public:
  BluezProfileManagerInterfaceHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluezProfileManagerInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_BLUEZ_INTERFACE_HANDLER_H_
