// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_DBUS_ADAPTOR_H_
#define SHILL_SERVICE_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/flimflam-service.h"

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
  static const char kInterfaceName[];
  static const char kPath[];

  ServiceDBusAdaptor(DBus::Connection* conn, Service *service);
  virtual ~ServiceDBusAdaptor();

  // Implementation of ServiceAdaptorInterface.
  void UpdateConnected();
  void EmitBoolChanged(const std::string& name, bool value);
  void EmitUintChanged(const std::string& name, uint32 value);
  void EmitIntChanged(const std::string& name, int value);
  void EmitStringChanged(const std::string& name, const std::string& value);

  // Implementation of Service_adaptor
  std::map<std::string, ::DBus::Variant> GetProperties(::DBus::Error &error);
  void SetProperty(const std::string& ,
                   const ::DBus::Variant& ,
                   ::DBus::Error &error);
  void ClearProperty(const std::string& , ::DBus::Error &error);
  void Connect(::DBus::Error &error);
  void Disconnect(::DBus::Error &error);
  void Remove(::DBus::Error &error);
  void MoveBefore(const ::DBus::Path& , ::DBus::Error &error);
  void MoveAfter(const ::DBus::Path& , ::DBus::Error &error);
  void ActivateCellularModem(const std::string& , ::DBus::Error &error);

 private:
  Service *service_;
  DISALLOW_COPY_AND_ASSIGN(ServiceDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_SERVICE_DBUS_ADAPTOR_H_
