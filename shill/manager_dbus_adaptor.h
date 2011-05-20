// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_DBUS_ADAPTOR_H_
#define SHILL_MANAGER_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/flimflam-manager.h"

namespace shill {
class Manager;

// Subclass of DBusAdaptor for Manager objects
class ManagerDBusAdaptor : public org::chromium::flimflam::Manager_adaptor,
                           public DBusAdaptor,
                           public ManagerAdaptorInterface {
 public:
  static const char kInterfaceName[];
  static const char kPath[];

  ManagerDBusAdaptor(DBus::Connection& conn, Manager *manager);
  virtual ~ManagerDBusAdaptor();
  void UpdateRunning();

  // Implementation of Manager_adaptor
  std::map<std::string, ::DBus::Variant> GetProperties(::DBus::Error &error);
  void SetProperty(const std::string& name,
                   const ::DBus::Variant& value,
                   ::DBus::Error &error);
  std::string GetState(::DBus::Error &error);
  ::DBus::Path CreateProfile(const std::string& name, ::DBus::Error &error);
  void RemoveProfile(const ::DBus::Path& path, ::DBus::Error &error);
  void RequestScan(const std::string& , ::DBus::Error &error);

  void EnableTechnology(const std::string& , ::DBus::Error &error);
  void DisableTechnology(const std::string& , ::DBus::Error &error);

  ::DBus::Path GetService(const std::map<std::string, ::DBus::Variant>& ,
                          ::DBus::Error &error);
  ::DBus::Path GetWifiService(const std::map<std::string, ::DBus::Variant>& ,
                              ::DBus::Error &error);
  void ConfigureWifiService(const std::map<std::string, ::DBus::Variant>& ,
                            ::DBus::Error &error);

  void RegisterAgent(const ::DBus::Path& , ::DBus::Error &error);
  void UnregisterAgent(const ::DBus::Path& , ::DBus::Error &error);

  std::string GetDebugTags(::DBus::Error &error);
  void SetDebugTags(const std::string& , ::DBus::Error &error);
  std::string ListDebugTags(::DBus::Error &error);
  uint32_t GetDebugMask(::DBus::Error &error);
  void SetDebugMask(const uint32_t& , ::DBus::Error &error);

  std::string GetServiceOrder(::DBus::Error &error);
  void SetServiceOrder(const std::string& , ::DBus::Error &error);

 private:
  Manager *manager_;
  DISALLOW_COPY_AND_ASSIGN(ManagerDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_MANAGER_DBUS_ADAPTOR_H_
