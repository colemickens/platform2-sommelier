//
// Copyright (C) 2015 The Android Open Source Project
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

#ifndef SHILL_DBUS_CHROMEOS_MANAGER_DBUS_ADAPTOR_H_
#define SHILL_DBUS_CHROMEOS_MANAGER_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/adaptor_interfaces.h"
#include "shill/chromeos_dbus_adaptors/org.chromium.flimflam.Manager.h"
#include "shill/dbus/chromeos_dbus_adaptor.h"

namespace shill {

class Manager;

// Subclass of DBusAdaptor for Manager objects
// There is a 1:1 mapping between Manager and ChromeosManagerDBusAdaptor
// instances.  Furthermore, the Manager owns the ChromeosManagerDBusAdaptor
// and manages its lifetime, so we're OK with ChromeosManagerDBusAdaptor
// having a bare pointer to its owner manager.
class ChromeosManagerDBusAdaptor
    : public org::chromium::flimflam::ManagerAdaptor,
      public org::chromium::flimflam::ManagerInterface,
      public ChromeosDBusAdaptor,
      public ManagerAdaptorInterface {
 public:
  static const char kPath[];

  ChromeosManagerDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                             Manager* manager);
  ~ChromeosManagerDBusAdaptor() override;

  // Implementation of ManagerAdaptorInterface.
  void RegisterAsync(
      const base::Callback<void(bool)>& completion_callback) override;
  const std::string& GetRpcIdentifier() override { return dbus_path().value(); }
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
  bool GetProperties(chromeos::ErrorPtr* error,
                     chromeos::VariantDictionary* properties) override;
  bool SetProperty(chromeos::ErrorPtr* error,
                   const std::string& name,
                   const chromeos::Any& value) override;
  bool GetState(chromeos::ErrorPtr* error, std::string* state) override;
  bool CreateProfile(chromeos::ErrorPtr* error,
                     const std::string& name,
                     dbus::ObjectPath* profile_path) override;
  bool RemoveProfile(chromeos::ErrorPtr* error,
                     const std::string& name) override;
  bool PushProfile(chromeos::ErrorPtr* error,
                   const std::string& name,
                   dbus::ObjectPath* profile_path) override;
  bool InsertUserProfile(chromeos::ErrorPtr* error,
                         const std::string& name,
                         const std::string& user_hash,
                         dbus::ObjectPath* profile_path) override;
  bool PopProfile(chromeos::ErrorPtr* error, const std::string& name) override;
  bool PopAnyProfile(chromeos::ErrorPtr* error) override;
  bool PopAllUserProfiles(chromeos::ErrorPtr* error) override;
  bool RecheckPortal(chromeos::ErrorPtr* error) override;
  bool RequestScan(chromeos::ErrorPtr* error,
                   const std::string& technology) override;
  void EnableTechnology(DBusMethodResponsePtr<> response,
                        const std::string& technology_namer) override;
  void DisableTechnology(DBusMethodResponsePtr<> response,
                         const std::string& technology_name) override;
  bool GetService(chromeos::ErrorPtr* error,
                  const chromeos::VariantDictionary& args,
                  dbus::ObjectPath* service_path) override;
  bool GetVPNService(chromeos::ErrorPtr* error,
                     const chromeos::VariantDictionary& args,
                     dbus::ObjectPath* service_path) override;
  bool GetWifiService(chromeos::ErrorPtr* error,
                      const chromeos::VariantDictionary& args,
                      dbus::ObjectPath* service_path) override;
  bool ConfigureService(chromeos::ErrorPtr* error,
                        const chromeos::VariantDictionary& args,
                        dbus::ObjectPath* service_path) override;
  bool ConfigureServiceForProfile(chromeos::ErrorPtr* error,
                                  const dbus::ObjectPath& profile_rpcid,
                                  const chromeos::VariantDictionary& args,
                                  dbus::ObjectPath* service_path) override;
  bool FindMatchingService(chromeos::ErrorPtr* error,
                           const chromeos::VariantDictionary& args,
                           dbus::ObjectPath* service_path) override;
  bool GetDebugLevel(chromeos::ErrorPtr* error,
                     int32_t* level) override;
  bool SetDebugLevel(chromeos::ErrorPtr* error, int32_t level) override;
  bool GetServiceOrder(chromeos::ErrorPtr* error, std::string* order) override;
  bool SetServiceOrder(chromeos::ErrorPtr* error,
                       const std::string& order) override;
  bool GetDebugTags(chromeos::ErrorPtr* error, std::string* tags) override;
  bool SetDebugTags(chromeos::ErrorPtr* error,
                    const std::string& tags) override;
  bool ListDebugTags(chromeos::ErrorPtr* error, std::string* tags) override;
  bool GetNetworksForGeolocation(
      chromeos::ErrorPtr* error,
      chromeos::VariantDictionary* networks) override;
  void VerifyDestination(DBusMethodResponsePtr<bool> response,
                         const std::string& certificate,
                         const std::string& public_key,
                         const std::string& nonce,
                         const std::string& signed_data,
                         const std::string& destination_udn,
                         const std::string& hotspot_ssid,
                         const std::string& hotspot_bssid) override;
  void VerifyAndEncryptCredentials(DBusMethodResponsePtr<std::string> response,
                                   const std::string& certificate,
                                   const std::string& public_key,
                                   const std::string& nonce,
                                   const std::string& signed_data,
                                   const std::string& destination_udn,
                                   const std::string& hotspot_ssid,
                                   const std::string& hotspot_bssid,
                                   const dbus::ObjectPath& network) override;
  void VerifyAndEncryptData(DBusMethodResponsePtr<std::string> response,
                            const std::string& certificate,
                            const std::string& public_key,
                            const std::string& nonce,
                            const std::string& signed_data,
                            const std::string& destination_udn,
                            const std::string& hotspot_ssid,
                            const std::string& hotspot_bssid,
                            const std::string& data) override;
  bool ConnectToBestServices(chromeos::ErrorPtr* error) override;
  bool CreateConnectivityReport(chromeos::ErrorPtr* error) override;
  bool ClaimInterface(chromeos::ErrorPtr* error,
                      dbus::Message* message,
                      const std::string& claimer_name,
                      const std::string& interface_name) override;
  bool ReleaseInterface(chromeos::ErrorPtr* error,
                        const std::string& claimer_name,
                        const std::string& interface_name) override;
  bool SetSchedScan(chromeos::ErrorPtr* error, bool enable) override;

 private:
  Manager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosManagerDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_MANAGER_DBUS_ADAPTOR_H_
