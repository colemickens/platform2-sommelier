//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DEVICE_DBUS_ADAPTOR_H_
#define SHILL_DEVICE_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Device.h"

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

  DeviceDBusAdaptor(DBus::Connection* conn, Device* device);
  ~DeviceDBusAdaptor() override;

  // Implementation of DeviceAdaptorInterface.
  const std::string& GetRpcIdentifier() override;
  const std::string& GetRpcConnectionIdentifier() override;
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitUint16Changed(const std::string& name, uint16_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;
  void EmitStringmapChanged(const std::string& name,
                            const Stringmap& value) override;
  void EmitStringmapsChanged(const std::string& name,
                             const Stringmaps& value) override;
  void EmitStringsChanged(const std::string& name,
                          const Strings& value) override;
  void EmitKeyValueStoreChanged(const std::string& name,
                                const KeyValueStore& value) override;
  void EmitRpcIdentifierChanged(const std::string& name,
                                const std::string& value) override;
  void EmitRpcIdentifierArrayChanged(
      const std::string& name, const std::vector<std::string>& value) override;

  // Implementation of Device_adaptor.
  std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error& error) override;  // NOLINT
  void SetProperty(const std::string& name,
                   const DBus::Variant& value,
                   DBus::Error& error) override;  // NOLINT
  void ClearProperty(const std::string& name,
                     DBus::Error& error) override;  // NOLINT
  void Enable(DBus::Error& error) override;  // NOLINT
  void Disable(DBus::Error& error) override;  // NOLINT
  void ProposeScan(DBus::Error& error) override;  // NOLINT
  DBus::Path AddIPConfig(const std::string& method,
                         DBus::Error& error) override;  // NOLINT
  void Register(const std::string& network_id,
                DBus::Error& error) override;  // NOLINT
  void RequirePin(const std::string& pin,
                  const bool& require,
                  DBus::Error& error) override;  // NOLINT
  void EnterPin(const std::string& pin, DBus::Error& error) override;  // NOLINT
  void UnblockPin(const std::string& unblock_code,
                  const std::string& pin,
                  DBus::Error& error) override;  // NOLINT
  void ChangePin(const std::string& old_pin,
                 const std::string& new_pin,
                 DBus::Error& error) override;  // NOLINT
  std::string PerformTDLSOperation(const std::string& operation,
                                   const std::string& peer,
                                   DBus::Error& error) override;  // NOLINT
  void Reset(DBus::Error& error) override;  // NOLINT
  void ResetByteCounters(DBus::Error& error) override;  // NOLINT
  void SetCarrier(const std::string& carrier,
                  DBus::Error& error) override;  // NOLINT
  void RequestRoam(const std::string& addr,
                   DBus::Error& error) override;  // NOLINT


  void AddWakeOnPacketConnection(const std::string& ip_endpoint,
                                 DBus::Error& error) override;  // NOLINT
  void RemoveWakeOnPacketConnection(const std::string& ip_endpoint,
                                    DBus::Error& error) override;  // NOLINT
  void RemoveAllWakeOnPacketConnections(DBus::Error& error) override;  // NOLINT

 private:
  Device* device_;
  const std::string connection_name_;
  DISALLOW_COPY_AND_ASSIGN(DeviceDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DEVICE_DBUS_ADAPTOR_H_
