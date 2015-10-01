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

#ifndef PROXY_SHILL_WIFI_CLIENT_H
#define PROXY_SHILL_WIFI_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <string>

#include <chromeos/any.h>
// Abstract class which defines the interface for the RPC server to talk to Shill.
// This helps in abstracting out the underlying protocol that Shill client
// needs to use: Dbus, Binder, etc.
// TODO: Need to come up with comments explaining what each method needs to do here.
class ProxyShillWifiClient {
 public:
  enum AutoConnectType {
    AUTO_CONNECT_TYPE_UNSPECIFIED,
    AUTO_CONNECT_TYPE_ENABLED,
    AUTO_CONNECT_TYPE_DISABLED
  };
  enum StationType {
    STATION_TYPE_IBSS,
    STATION_TYPE_MANAGED,
    STATION_TYPE_DEFAULT = STATION_TYPE_MANAGED
  };

  ProxyShillWifiClient() = default;
  virtual void SetLoggingForWifiTest() = 0;
  virtual void RemoveAllWifiEntries() = 0;
  virtual void ConfigureWifiService(std::string ssid,
                                    std::string security,
                                    chromeos::VariantDictionary& security_parameters,
                                    bool save_credentials,
                                    StationType station_type,
                                    bool hidden_network,
                                    std::string guid,
                                    AutoConnectType autoconnect) = 0;
  virtual bool ConnectToWifiNetwork(std::string ssid,
                                    std::string security,
                                    chromeos::VariantDictionary& security_parameters,
                                    bool save_credentials,
                                    StationType station_type,
                                    bool hidden_network,
                                    std::string guid,
                                    AutoConnectType autoconnect,
                                    int discovery_timeout_seconds,
                                    int association_timeout_seconds,
                                    int configuration_timeout_seconds,
                                    int& discovery_time,
                                    int& association_time,
                                    int& configuration_time,
                                    int& failure_reason) = 0;
  virtual bool DisconnectFromWifiNetwork(std::string ssid,
                                         int discovery_timeout_seconds,
                                         int& disconnect_time) = 0;
  virtual bool ConfigureBgSan(std::string interface,
                              std::string method_name,
                              int short_interval,
                              int long_interval,
                              int signal) = 0;
  virtual std::vector<std::string> GetActiveWifiSSIDs() = 0;
  virtual bool WaitForServiceStates(std::string ssid,
                                    const std::vector<std::string>& expected_states,
                                    const int timeout_seconds,
                                    std::string& final_state,
                                    int& time) = 0;
  virtual bool CreateProfile(std::string profile_name) = 0;
  virtual bool PushProfile(std::string profile_name) = 0;
  virtual bool PopProfile(std::string profile_name) = 0;
  virtual bool RemoveProfile(std::string profile_name) = 0;
  virtual bool CleanProfiles() = 0;
  virtual bool DeleteEntriesForSsid(std::string ssid) = 0;
  virtual std::vector<std::string> ListControlledWifiInterfaces() = 0;
  virtual bool Disconnect(std::string ssid) = 0;
  virtual std::string GetServiceOrder() = 0;
  virtual bool SetServiceOrder(std::string service_order) = 0;
  virtual bool SetSchedScan(bool enable) = 0;
  virtual std::string GetPropertyOnDevice(std::string interface_name,
                                          std::string property_name) = 0;
  virtual bool SetPropertyOnDevice(std::string interface_name,
                                   std::string property_name,
                                   std::string property_value) = 0;
  virtual bool RequestRoam(std::string bssid, std::string interface_name) = 0;
  virtual bool SetDeviceEnabled(std::string interface_name, bool enable) = 0;
  virtual bool DiscoverTDLSLink(std::string interface_name,
                                std::string peer_mac_address) = 0;
  virtual bool EstablishTDLSLink(std::string interface_name,
                                 std::string peer_mac_address) = 0;
  virtual bool QueryTDLSLink(std::string interface_name,
                             std::string peer_mac_address) = 0;
  virtual bool AddWakePacketSource(std::string interface_name,
                                   std::string source_ip_address) = 0;
  virtual bool RemoveWakePacketSource(std::string interface_name,
                                      std::string source_ip_address) = 0;
  virtual bool RemoveAllWakePacketSources(std::string interface_name) = 0;
  virtual ~ProxyShillWifiClient() = default;
};

#endif // PROXY_SHILL_WIFI_CLIENT_H
