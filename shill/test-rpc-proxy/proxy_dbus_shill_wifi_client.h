//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License") override;
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

#ifndef PROXY_DBUS_SHILL_WIFI_CLIENT_H
#define PROXY_DBUS_SHILL_WIFI_CLIENT_H

#include "proxy_dbus_client.h"
#include "proxy_shill_wifi_client.h"

// This is the class implementing the ShillWifiClient abstract interface for a Dbus
// Client.
class ProxyDbusShillWifiClient : public ProxyShillWifiClient {
 public:
  ProxyDbusShillWifiClient(scoped_refptr<dbus::Bus> dbus_bus);
  ~ProxyDbusShillWifiClient() override = default;
  bool SetLogging() override;
  bool RemoveAllWifiEntries() override;
  bool ConfigureServiceByGuid(const std::string& guid,
                              AutoConnectType autoconnect,
                              const std::string& passphrase) override;
  bool ConfigureWifiService(const std::string& ssid,
                            const std::string& security,
                            const brillo::VariantDictionary& security_params,
                            bool save_credentials,
                            StationType station_type,
                            bool hidden_network,
                            const std::string& guid,
                            AutoConnectType autoconnect) override;
  bool ConnectToWifiNetwork(const std::string& ssid,
                            const std::string& security,
                            const brillo::VariantDictionary& security_params,
                            bool save_credentials,
                            StationType station_type,
                            bool hidden_network,
                            const std::string& guid,
                            AutoConnectType autoconnect,
                            long discovery_timeout_milliseconds,
                            long association_timeout_milliseconds,
                            long configuration_timeout_milliseconds,
                            long* discovery_time_milliseconds,
                            long* association_time_milliseconds,
                            long* configuration_time_milliseconds,
                            std::string* failure_reason) override;
  bool DisconnectFromWifiNetwork(std::string ssid,
                                 int discovery_timeout_seconds,
                                 int& disconnect_time) override;
  bool ConfigureBgSan(std::string interface,
                      std::string method_name,
                      int short_interval,
                      int long_interval,
                      int signal) override;
  std::vector<std::string> GetActiveWifiSSIDs() override;
  bool WaitForServiceStates(std::string ssid,
                            const std::vector<std::string>& expected_states,
                            const int timeout_seconds,
                            std::string& final_state,
                            int& time) override;
  bool CreateProfile(std::string profile_name) override;
  bool PushProfile(std::string profile_name) override;
  bool PopProfile(std::string profile_name) override;
  bool RemoveProfile(std::string profile_name) override;
  bool CleanProfiles() override;
  bool DeleteEntriesForSsid(std::string ssid) override;
  std::vector<std::string> ListControlledWifiInterfaces() override;
  bool Disconnect(std::string ssid) override;
  std::string GetServiceOrder() override;
  bool SetServiceOrder(std::string service_order) override;
  bool SetSchedScan(bool enable) override;
  std::string GetPropertyOnDevice(std::string interface_name,
                                  std::string property_name) override;
  bool SetPropertyOnDevice(std::string interface_name,
                           std::string property_name,
                           std::string property_value) override;
  bool RequestRoam(std::string bssid, std::string interface_name) override;
  bool SetDeviceEnabled(std::string interface_name, bool enable) override;
  bool DiscoverTDLSLink(std::string interface_name,
                        std::string peer_mac_address) override;
  bool EstablishTDLSLink(std::string interface_name,
                         std::string peer_mac_address) override;
  bool QueryTDLSLink(std::string interface_name,
                     std::string peer_mac_address) override;
  bool AddWakePacketSource(std::string interface_name,
                           std::string source_ip_address) override;
  bool RemoveWakePacketSource(std::string interface_name,
                              std::string source_ip_address) override;
  bool RemoveAllWakePacketSources(std::string interface_name) override;

 private:
  void SetAutoConnectInServiceParams(AutoConnectType autoconnect,
                                     brillo::VariantDictionary* service_params);
  std::unique_ptr<ProxyDbusClient> dbus_client_;
};

#endif // PROXY_DBUS_SHILL_WIFI_CLIENT_H
