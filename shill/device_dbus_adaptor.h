// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_DBUS_ADAPTOR_H_
#define SHILL_DEVICE_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_bindings/flimflam-device.h"

namespace shill {

class Device;

// Subclass of DBusAdaptor for Device objects
// There is a 1:1 mapping between Device and DeviceDBusAdaptor instances.
// Furthermore, the Device owns the DeviceDBusAdaptor and manages its lifetime,
// so we're OK with DeviceDBusAdaptor having a bare pointer to its owner device.
class DeviceDBusAdaptor : public org::chromium::flimflam::Device_adaptor,
                          public DBusAdaptor,
                          public DeviceAdaptorInterface {
 public:
  static const char kPath[];

  DeviceDBusAdaptor(DBus::Connection* conn, Device *device);
  virtual ~DeviceDBusAdaptor();

  // Implementation of DeviceAdaptorInterface.
  virtual const std::string &GetRpcIdentifier();
  virtual const std::string &GetRpcConnectionIdentifier();
  virtual void UpdateEnabled();
  virtual void EmitBoolChanged(const std::string& name, bool value);
  virtual void EmitUintChanged(const std::string& name, uint32 value);
  virtual void EmitIntChanged(const std::string& name, int value);
  virtual void EmitStringChanged(const std::string& name,
                                 const std::string& value);
  virtual void EmitStringmapsChanged(const std::string &name,
                                     const Stringmaps &value);
  virtual void EmitKeyValueStoreChanged(const std::string &name,
                                        const KeyValueStore &value);

  // Implementation of Device_adaptor.
  virtual std::map<std::string, ::DBus::Variant> GetProperties(
      ::DBus::Error &error);
  virtual void SetProperty(const std::string& name,
                           const ::DBus::Variant& value,
                           ::DBus::Error &error);
  virtual void ClearProperty(const std::string& , ::DBus::Error &error);
  virtual void Enable(::DBus::Error &error);
  virtual void Disable(::DBus::Error &error);
  virtual void ProposeScan(::DBus::Error &error);
  virtual ::DBus::Path AddIPConfig(const std::string& , ::DBus::Error &error);
  virtual void Register(const std::string &network_id, ::DBus::Error &error);
  virtual void RequirePin(const std::string &pin,
                          const bool &require,
                          ::DBus::Error &error);
  virtual void EnterPin(const std::string &pin, ::DBus::Error &error);
  virtual void UnblockPin(const std::string &unblock_code,
                          const std::string &pin,
                          ::DBus::Error &error);
  virtual void ChangePin(const std::string &old_pin,
                         const std::string &new_pin,
                         ::DBus::Error &error);
  virtual void ResetByteCounters(::DBus::Error &error);

 private:
  Device *device_;
  const std::string connection_name_;
  DISALLOW_COPY_AND_ASSIGN(DeviceDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_DEVICE_DBUS_ADAPTOR_H_
