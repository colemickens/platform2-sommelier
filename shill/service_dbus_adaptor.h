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

  ServiceDBusAdaptor(DBus::Connection *conn, Service *service);
  ~ServiceDBusAdaptor() override;

  // Implementation of ServiceAdaptorInterface.
  virtual const std::string &GetRpcIdentifier() { return path(); }
  virtual void UpdateConnected();
  virtual void EmitBoolChanged(const std::string &name, bool value);
  virtual void EmitUint8Changed(const std::string &name, uint8_t value);
  virtual void EmitUint16Changed(const std::string &name, uint16_t value);
  virtual void EmitUint16sChanged(const std::string &name,
                                  const Uint16s &value);
  virtual void EmitUintChanged(const std::string &name, uint32_t value);
  virtual void EmitIntChanged(const std::string &name, int value);
  virtual void EmitRpcIdentifierChanged(
      const std::string &name, const std::string &value);
  virtual void EmitStringChanged(
      const std::string &name, const std::string &value);
  virtual void EmitStringmapChanged(const std::string &name,
                                    const Stringmap &value);

  // Implementation of Service_adaptor
  std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error &error);  // NOLINT
  void SetProperty(const std::string &name,
                   const DBus::Variant &value,
                   DBus::Error &error);  // NOLINT
  void SetProperties(const std::map<std::string, DBus::Variant> &args,
                     DBus::Error &error);  // NOLINT
  void ClearProperty(const std::string &name, DBus::Error &error);  // NOLINT
  std::vector<bool> ClearProperties(const std::vector<std::string> &names,
                                    DBus::Error &error);  // NOLINT
  void Connect(DBus::Error &error);  // NOLINT
  void Disconnect(DBus::Error &error);  // NOLINT
  void Remove(DBus::Error &error);  // NOLINT
  void ActivateCellularModem(const std::string &carrier,
                             DBus::Error &error);  // NOLINT
  void CompleteCellularActivation(DBus::Error &error);  // NOLINT
  std::map<DBus::Path, std::string> GetLoadableProfileEntries(
      DBus::Error &error);  // NOLINT

 private:
  Service *service_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_SERVICE_DBUS_ADAPTOR_H_
