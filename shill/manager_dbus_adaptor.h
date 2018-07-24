// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_DBUS_ADAPTOR_H_
#define SHILL_MANAGER_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Manager.h"

namespace shill {

class Manager;

// Subclass of DBusAdaptor for Manager objects
// There is a 1:1 mapping between Manager and ManagerDBusAdaptor
// instances.  Furthermore, the Manager owns the ManagerDBusAdaptor
// and manages its lifetime, so we're OK with ManagerDBusAdaptor
// having a bare pointer to its owner manager.
class ManagerDBusAdaptor : public org::chromium::flimflam::Manager_adaptor,
                           public DBusAdaptor,
                           public ManagerAdaptorInterface,
                           public base::SupportsWeakPtr<ManagerDBusAdaptor> {
 public:
  static const char kPath[];

  ManagerDBusAdaptor(DBus::Connection* conn, Manager* manager);
  ~ManagerDBusAdaptor() override;

  // Implementation of ManagerAdaptorInterface.
  const std::string& GetRpcIdentifier() override { return path(); }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;
  void EmitStringsChanged(const std::string& name,
                          const std::vector<std::string>& value) override;
  void EmitRpcIdentifierChanged(
      const std::string& name, const std::string& value) override;
  void EmitRpcIdentifierArrayChanged(
      const std::string& name, const std::vector<std::string>& value) override;
  void EmitStateChanged(const std::string& new_state) override;

  // Implementation of Manager_adaptor
  std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error& error) override;  // NOLINT
  void SetProperty(const std::string& name,
                   const DBus::Variant& value,
                   DBus::Error& error) override;  // NOLINT
  std::string GetState(DBus::Error& error) override;  // NOLINT
  DBus::Path CreateProfile(const std::string& name,
                           DBus::Error& error) override;  // NOLINT
  void RemoveProfile(const std::string& name,
                     DBus::Error& error) override;  // NOLINT
  DBus::Path PushProfile(const std::string& name,
                         DBus::Error& error) override;  // NOLINT
  DBus::Path InsertUserProfile(const std::string& name,
                               const std::string& user_hash,
                               DBus::Error& error) override;  // NOLINT
  void PopProfile(const std::string& name,
                  DBus::Error& error) override;  // NOLINT
  void PopAnyProfile(DBus::Error& error) override;  // NOLINT
  void PopAllUserProfiles(DBus::Error& error) override;  // NOLINT
  void RecheckPortal(DBus::Error& error) override;  // NOLINT
  void RequestScan(const std::string& technology,
                   DBus::Error& error) override;  // NOLINT

  void EnableTechnology(const std::string& technology_name,
                        DBus::Error& error) override;  // NOLINT
  void DisableTechnology(const std::string& technology_name,
                         DBus::Error& error) override;  // NOLINT

  DBus::Path GetService(const std::map<std::string, DBus::Variant>& args,
                        DBus::Error& error) override;  // NOLINT
  DBus::Path GetVPNService(const std::map<std::string, DBus::Variant>& args,
                           DBus::Error& error) override;  // NOLINT
  DBus::Path GetWifiService(
      const std::map<std::string, DBus::Variant>& args,
      DBus::Error& error) override;  // NOLINT
  DBus::Path ConfigureService(
      const std::map<std::string, DBus::Variant>& args,
      DBus::Error& error) override;  // NOLINT
  DBus::Path ConfigureServiceForProfile(
      const DBus::Path& profile_rpcid,
      const std::map<std::string, DBus::Variant>& args,
      DBus::Error& error) override;  // NOLINT
  DBus::Path FindMatchingService(
      const std::map<std::string, DBus::Variant>& args,
      DBus::Error& error) override;  // NOLINT

  int32_t GetDebugLevel(DBus::Error& error) override;  // NOLINT
  void SetDebugLevel(const int32_t& level,
                     DBus::Error& error) override;  // NOLINT

  std::string GetServiceOrder(DBus::Error& error) override;  // NOLINT
  void SetServiceOrder(const std::string& order,
                       DBus::Error& error) override;  // NOLINT

  std::string GetDebugTags(DBus::Error& error) override;  // NOLINT
  void SetDebugTags(const std::string& tags,
                    DBus::Error& error) override;  // NOLINT
  std::string ListDebugTags(DBus::Error& error) override;  // NOLINT

  std::map<std::string, DBus::Variant> GetNetworksForGeolocation(
      DBus::Error& error) override;  // NOLINT

  bool VerifyDestination(const std::string& certificate,
                         const std::string& public_key,
                         const std::string& nonce,
                         const std::string& signed_data,
                         const std::string& destination_udn,
                         const std::string& hotspot_ssid,
                         const std::string& hotspot_bssid,
                         DBus::Error& error) override;  // NOLINT

  std::string VerifyAndEncryptCredentials(
      const std::string& certificate,
      const std::string& public_key,
      const std::string& nonce,
      const std::string& signed_data,
      const std::string& destination_udn,
      const std::string& hotspot_ssid,
      const std::string& hotspot_bssid,
      const DBus::Path& path,
      DBus::Error& error) override;  // NOLINT

  std::string VerifyAndEncryptData(const std::string& certificate,
                                   const std::string& public_key,
                                   const std::string& nonce,
                                   const std::string& signed_data,
                                   const std::string& destination_udn,
                                   const std::string& hotspot_ssid,
                                   const std::string& hotspot_bssid,
                                   const std::string& data,
                                   DBus::Error& error) override;  // NOLINT

  void ConnectToBestServices(DBus::Error& error) override;  // NOLINT

  void CreateConnectivityReport(DBus::Error& error) override;  // NOLINT

  void ClaimInterface(const std::string& claimer_name,
                      const std::string& interface_name,
                      DBus::Error& error) override;  // NOLINT
  void ReleaseInterface(const std::string& claimer_name,
                        const std::string& interface_name,
                        DBus::Error& error) override;  // NOLINT

  void SetSchedScan(const bool& enable,
                    DBus::Error& error) override;  // NOLINT

 private:
  Manager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ManagerDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_MANAGER_DBUS_ADAPTOR_H_
