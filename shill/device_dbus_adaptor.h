// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
  static const char kInterfaceName[];
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

  // Implementation of Device_adaptor.
  std::map<std::string, ::DBus::Variant> GetProperties(::DBus::Error &error);
  void SetProperty(const std::string& name,
                   const ::DBus::Variant& value,
                   ::DBus::Error &error);
  void ClearProperty(const std::string& , ::DBus::Error &error);
  void ProposeScan(::DBus::Error &error);
  ::DBus::Path AddIPConfig(const std::string& , ::DBus::Error &error);
  void Register(const std::string& , ::DBus::Error &error);
  void RequirePin(const std::string& , const bool& , ::DBus::Error &error);
  void EnterPin(const std::string& , ::DBus::Error &error);
  void UnblockPin(const std::string& ,
                  const std::string& ,
                  ::DBus::Error &error);
  void ChangePin(const std::string& ,
                 const std::string& ,
                 ::DBus::Error &error);

 private:
  Device *device_;
  const std::string connection_name_;
  DISALLOW_COPY_AND_ASSIGN(DeviceDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_DEVICE_DBUS_ADAPTOR_H_
