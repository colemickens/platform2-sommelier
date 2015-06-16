// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IPCONFIG_DBUS_ADAPTOR_H_
#define SHILL_IPCONFIG_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.IPConfig.h"

namespace shill {

class IPConfig;

// Subclass of DBusAdaptor for IPConfig objects
// There is a 1:1 mapping between IPConfig and IPConfigDBusAdaptor
// instances.  Furthermore, the IPConfig owns the IPConfigDBusAdaptor
// and manages its lifetime, so we're OK with IPConfigDBusAdaptor
// having a bare pointer to its owner ipconfig.
class IPConfigDBusAdaptor : public org::chromium::flimflam::IPConfig_adaptor,
                            public DBusAdaptor,
                            public IPConfigAdaptorInterface {
 public:
  static const char kInterfaceName[];
  static const char kPath[];

  IPConfigDBusAdaptor(DBus::Connection* conn, IPConfig* ipconfig);
  ~IPConfigDBusAdaptor() override;

  // Implementation of IPConfigAdaptorInterface.
  const std::string& GetRpcIdentifier() override { return path(); }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;
  void EmitStringsChanged(const std::string& name,
                          const std::vector<std::string>& value) override;

  // Implementation of IPConfig_adaptor
  std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error& error) override;  // NOLINT
  void SetProperty(const std::string& name,
                   const DBus::Variant& value,
                   DBus::Error& error) override;  // NOLINT
  void ClearProperty(const std::string& name,
                     DBus::Error& error) override;  // NOLINT
  void Remove(DBus::Error& error) override;  // NOLINT
  void Refresh(DBus::Error& error) override;  // NOLINT

 private:
  IPConfig* ipconfig_;
  DISALLOW_COPY_AND_ASSIGN(IPConfigDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_IPCONFIG_DBUS_ADAPTOR_H_
