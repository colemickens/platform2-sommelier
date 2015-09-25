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
#include "proxy_dbus_client.h"

void ProxyDbusClient::ConfigureAndConnectWifi(
    std::string ssid, bool is_hex_ssid, std::string psk) {

  shill_manager_proxy_.reset(
      new org::chromium::flimflam::ManagerProxy(proxy_bus_));
  dbus::ObjectPath created_service;
  chromeos::ErrorPtr configure_error;
  if (!shill_manager_proxy_->ConfigureService(
          GetWifiServiceConfig(ssid, is_hex_ssid, psk),
          &created_service,
          &configure_error)) {
    LOG(ERROR) << "Configure service failed";
    return;
  }

  shill_service_proxy_.reset(
      new org::chromium::flimflam::ServiceProxy(proxy_bus_, created_service));
  chromeos::ErrorPtr connect_error;
  if (!shill_service_proxy_->Connect(&connect_error)) {
    LOG(ERROR) << "Connect service failed";
    return;
  }

  return;
}

void ProxyDbusClient::DisconnectWifi() {
  chromeos::ErrorPtr disconnect_error;
  if (!shill_service_proxy_->Disconnect(&disconnect_error)) {
    LOG(ERROR) << "Connect service failed";
    return;
  }
}

std::map<std::string, chromeos::Any> ProxyDbusClient::GetWifiServiceConfig(
    std::string ssid, bool is_hex_ssid, std::string psk) {

  std::map<std::string, chromeos::Any> configure_dict;
  configure_dict[shill::kTypeProperty] = shill::kTypeWifi;
  if (is_hex_ssid) {
    configure_dict[shill::kWifiHexSsid] = ssid;
  } else {
    configure_dict[shill::kSSIDProperty] = ssid;
  }
  if (!psk.empty()) {
    configure_dict[shill::kPassphraseProperty] = psk;
    configure_dict[shill::kSecurityProperty] = shill::kSecurityPsk;
  }
  return configure_dict;
}
