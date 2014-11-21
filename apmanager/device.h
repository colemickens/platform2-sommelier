// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DEVICE_H_
#define APMANAGER_DEVICE_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <shill/net/byte_string.h>
#include <shill/net/nl80211_message.h>

#include "apmanager/dbus_adaptors/org.chromium.apmanager.Device.h"

namespace apmanager {

// Abstraction for WiFi Device (PHY). Each device can have one or more
// interfaces defined on it.
class Device : public base::RefCounted<Device>,
               public org::chromium::apmanager::DeviceAdaptor,
               public org::chromium::apmanager::DeviceInterface {
 public:
  struct WiFiInterface {
    WiFiInterface() : iface_index(0), iface_type(0) {}
    WiFiInterface(const std::string& in_iface_name,
                  const std::string& in_device_name,
                  uint32_t in_iface_index,
                  uint32_t in_iface_type)
        : iface_name(in_iface_name),
          device_name(in_device_name),
          iface_index(in_iface_index),
          iface_type(in_iface_type) {}
    std::string iface_name;
    std::string device_name;
    uint32_t iface_index;
    uint32_t iface_type;
    bool Equals(const WiFiInterface& other) const {
      return this->iface_name == other.iface_name &&
             this->device_name == other.device_name &&
             this->iface_index == other.iface_index &&
             this->iface_type == other.iface_type;
    }
  };

  Device();
  virtual ~Device();

  // Register Device DBus object.
  void RegisterAsync(
      chromeos::dbus_utils::ExportedObjectManager* object_manager,
      chromeos::dbus_utils::AsyncEventSequencer* sequencer,
      int device_identifier);

  // Register/deregister WiFi interface on this device.
  virtual void RegisterInterface(const WiFiInterface& interface);
  virtual void DeregisterInterface(const WiFiInterface& interface);

  // Parse device information from NL80211 message.
  void ParseWiFiPhyInfo(const shill::Nl80211Message& msg);

  // Function for claiming/releasing ownership of this device. This will invoke
  // dbus calls to shill to claim/release all the interfaces reside on this
  // device.
  virtual bool ClaimDevice();
  virtual bool ReleaseDevice();

  // Return true if interface with |interface_name| resides on this device,
  // false otherwise.
  virtual bool InterfaceExists(const std::string& interface_name);

 private:
  friend class DeviceTest;

  // Determine preferred interface to used for AP operation based on the list
  // of interfaces reside on this device
  void UpdatePreferredAPInterface();

  // List of WiFi interfaces live on this device (PHY).
  std::vector<WiFiInterface> interface_list_;

  dbus::ObjectPath dbus_path_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace apmanager

#endif  // APMANAGER_DEVICE_H_
