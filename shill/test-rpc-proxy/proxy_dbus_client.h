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

#ifndef PROXY_DBUS_CLIENT_H
#define PROXY_DBUS_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/any.h>
#if defined(__ANDROID__)
#include <dbus/service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__
#include <shill/dbus-proxies.h>

class ProxyDbusClient {
 public:
  ProxyDbusClient() : weak_ptr_factory_(this) {}

  void set_proxy_dbus(scoped_refptr<dbus::Bus> bus) {
    proxy_bus_ = bus;
  }

  void set_proxy_message_loop(base::MessageLoop *message_loop) {
    proxy_message_loop_ = message_loop;
  }

  /** Functions invoked from the RPC server thread **/
  void OnConnectWifiRPCRequest(
      std::string ssid, bool is_hex_ssid, std::string psk) {
    proxy_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ProxyDbusClient::ConfigureAndConnectWifi,
                   weak_ptr_factory_.GetWeakPtr(),
                   ssid,
                   is_hex_ssid,
                   psk));
  }
  void OnDisconnectWifiRPCRequest() {
    proxy_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ProxyDbusClient::DisconnectWifi,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  scoped_refptr<dbus::Bus> proxy_bus_;
  base::WeakPtrFactory<ProxyDbusClient> weak_ptr_factory_;
  base::MessageLoop *proxy_message_loop_;
  std::unique_ptr<org::chromium::flimflam::ManagerProxy> shill_manager_proxy_;
  std::unique_ptr<org::chromium::flimflam::ServiceProxy> shill_service_proxy_;

  // Configure and Connect to Wifi AP.
  void ConfigureAndConnectWifi(
      std::string ssid, bool is_hex_ssid, std::string psk);
  // Disconnect from Wifi AP.
  void DisconnectWifi();
  // Get Wifi service configuration to be passed on to Shill.
  std::map<std::string, chromeos::Any> GetWifiServiceConfig(
      std::string ssid, bool is_hex_ssid, std::string psk);

};
#endif //PROXY_DBUS_CLIENT_H
