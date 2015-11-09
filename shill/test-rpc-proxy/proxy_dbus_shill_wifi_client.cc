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

#include "proxy_dbus_shill_wifi_client.h"

ProxyDbusShillWifiClient::ProxyDbusShillWifiClient(
    scoped_refptr<dbus::Bus> dbus_bus) {
  dbus_client_.reset(new ProxyDbusClient(dbus_bus));
}

bool ProxyDbusShillWifiClient::SetLogging() {
  dbus_client_->SetLogging(ProxyDbusClient::TECHNOLOGY_WIFI);
  return true;
}

bool ProxyDbusShillWifiClient::RemoveAllWifiEntries() {
  for (auto& profile_proxy : dbus_client_->GetProfileProxies()) {
    brillo::Any property_value;
    CHECK(dbus_client_->GetPropertyValueFromProfileProxy(
          profile_proxy.get(), shill::kEntriesProperty, &property_value));
    auto entry_ids = property_value.Get<std::vector<std::string>>();
    for (const auto& entry_id : entry_ids) {
      brillo::VariantDictionary entry_props;
      if (profile_proxy->GetEntry(entry_id, &entry_props, nullptr)) {
        if (entry_props[shill::kTypeProperty].Get<std::string>() ==
            shill::kTypeWifi) {
          profile_proxy->DeleteEntry(entry_id, nullptr);
        }
      }
    }
  }
  return true;
}

void ProxyDbusShillWifiClient::ConfigureWifiService(
    std::string ssid,
    std::string security,
    brillo::VariantDictionary& security_parameters,
    bool save_credentials,
    StationType station_type,
    bool hidden_network,
    std::string guid,
    AutoConnectType autoconnect) {
}

bool ProxyDbusShillWifiClient::ConnectToWifiNetwork(
    std::string ssid,
    std::string security,
    brillo::VariantDictionary& security_parameters,
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
    int& failure_reason) {
  return true;
}

bool ProxyDbusShillWifiClient::DisconnectFromWifiNetwork(
    std::string ssid,
    int discovery_timeout_seconds,
    int& disconnect_time) {
  return true;
}

bool ProxyDbusShillWifiClient::ConfigureBgSan(
    std::string interface,
    std::string method_name,
    int short_interval,
    int long_interval,
    int signal) {
  return true;
}

std::vector<std::string> ProxyDbusShillWifiClient::GetActiveWifiSSIDs() {

  return std::vector<std::string>();
}

bool ProxyDbusShillWifiClient::WaitForServiceStates(
    std::string ssid,
    const std::vector<std::string>& expected_states,
    const int timeout_seconds,
    std::string& final_state,
    int& time) {
  return true;
}

bool ProxyDbusShillWifiClient::CreateProfile(std::string profile_name) {
  return dbus_client_->CreateProfile(profile_name);
}

bool ProxyDbusShillWifiClient::PushProfile(std::string profile_name) {
  return dbus_client_->PushProfile(profile_name);
}

bool ProxyDbusShillWifiClient::PopProfile(std::string profile_name) {
  if (profile_name.empty()) {
    return dbus_client_->PopAnyProfile();
  } else {
    return dbus_client_->PopProfile(profile_name);
  }
}

bool ProxyDbusShillWifiClient::RemoveProfile(std::string profile_name) {
  return dbus_client_->RemoveProfile(profile_name);
}

bool ProxyDbusShillWifiClient::CleanProfiles() {
  return true;
}

bool ProxyDbusShillWifiClient::DeleteEntriesForSsid(std::string ssid) {
  return true;
}

std::vector<std::string> ProxyDbusShillWifiClient::ListControlledWifiInterfaces() {
  return std::vector<std::string>();
}

bool ProxyDbusShillWifiClient::Disconnect(std::string ssid) {
  return true;
}

std::string ProxyDbusShillWifiClient::GetServiceOrder() {
  return std::string();
}

bool ProxyDbusShillWifiClient::SetServiceOrder(std::string service_order) {
  return true;
}

bool ProxyDbusShillWifiClient::SetSchedScan(bool enable) {
  return true;

}
std::string ProxyDbusShillWifiClient::GetPropertyOnDevice(
    std::string interface_name,
    std::string property_name) {
  return std::string();
}

bool ProxyDbusShillWifiClient::SetPropertyOnDevice(
    std::string interface_name,
    std::string property_name,
    std::string property_value) {
  return true;
}

bool ProxyDbusShillWifiClient::RequestRoam(
    std::string bssid,
    std::string interface_name) {

  return true;
}

bool ProxyDbusShillWifiClient::SetDeviceEnabled(
    std::string interface_name,
    bool enable) {
  return true;
}

bool ProxyDbusShillWifiClient::DiscoverTDLSLink(
    std::string interface_name,
    std::string peer_mac_address) {
  return true;
}

bool ProxyDbusShillWifiClient::EstablishTDLSLink(
    std::string interface_name,
    std::string peer_mac_address) {
  return true;
}

bool ProxyDbusShillWifiClient::QueryTDLSLink(
    std::string interface_name,
    std::string peer_mac_address) {
  return true;
}

bool ProxyDbusShillWifiClient::AddWakePacketSource(
    std::string interface_name,
    std::string source_ip_address) {
  return true;
}

bool ProxyDbusShillWifiClient::RemoveWakePacketSource(
    std::string interface_name,
    std::string source_ip_address) {
  return true;
}

bool ProxyDbusShillWifiClient::RemoveAllWakePacketSources(
    std::string interface_name) {
  return true;
}
