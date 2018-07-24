// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_DBUS_ADAPTOR_H_
#define SHILL_SERVICE_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Service.h"

namespace shill {

class Service;

// Subclass of DBusAdaptor for Service objects
// There is a 1:1 mapping between Service and ServiceDBusAdaptor
// instances.  Furthermore, the Service owns the ServiceDBusAdaptor
// and manages its lifetime, so we're OK with ServiceDBusAdaptor
// having a bare pointer to its owner service.
class ServiceDBusAdaptor : public org::chromium::flimflam::Service_adaptor,
                           public DBusAdaptor,
                           public ServiceAdaptorInterface {
 public:
  static const char kPath[];

  ServiceDBusAdaptor(DBus::Connection* conn, Service* service);
  ~ServiceDBusAdaptor() override;

  // Implementation of ServiceAdaptorInterface.
  const std::string& GetRpcIdentifier() override { return path(); }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUint8Changed(const std::string& name, uint8_t value) override;
  void EmitUint16Changed(const std::string& name, uint16_t value) override;
  void EmitUint16sChanged(const std::string& name,
                          const Uint16s& value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitRpcIdentifierChanged(
      const std::string& name, const std::string& value) override;
  void EmitStringChanged(
      const std::string& name, const std::string& value) override;
  void EmitStringmapChanged(const std::string& name,
                            const Stringmap& value) override;

  // Implementation of Service_adaptor
  std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error& error) override;  // NOLINT
  void SetProperty(const std::string& name,
                   const DBus::Variant& value,
                   DBus::Error& error) override;  // NOLINT
  void SetProperties(const std::map<std::string, DBus::Variant>& args,
                     DBus::Error& error) override;  // NOLINT
  void ClearProperty(const std::string& name,
                     DBus::Error& error) override;  // NOLINT
  std::vector<bool> ClearProperties(const std::vector<std::string>& names,
                                    DBus::Error& error) override;  // NOLINT
  void Connect(DBus::Error& error) override;  // NOLINT
  void Disconnect(DBus::Error& error) override;  // NOLINT
  void Remove(DBus::Error& error) override;  // NOLINT
  void ActivateCellularModem(const std::string& carrier,
                             DBus::Error& error) override;  // NOLINT
  void CompleteCellularActivation(DBus::Error& error) override;  // NOLINT
  std::map<DBus::Path, std::string> GetLoadableProfileEntries(
      DBus::Error& error) override;  // NOLINT

 private:
  Service* service_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_SERVICE_DBUS_ADAPTOR_H_
