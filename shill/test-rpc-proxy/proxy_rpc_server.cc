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

#include <base/bind.h>

#include "proxy_rpc_server.h"

namespace {
// XmlRpc library verbosity level.
static const int kDefaultXmlRpcVerbosity = 5;
// Profile name to be used for all the tests.
static const char kTestProfileName[] = "test";

bool ValidateNumOfElements(const XmlRpc::XmlRpcValue& value, int expected_num) {
  if (expected_num != 0) {
    return (value.valid() && value.size() == expected_num);
  } else {
    // |value| will be marked invalid when there are no elements.
    return !value.valid();
  }
}
}// namespace

/*************** RPC Method implementations **********/
XmlRpc::XmlRpcValue CreateProfile(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 1)) {
    return false;
  }
  const std::string& profile_name(params_in[0]);
  return shill_wifi_client->CreateProfile(profile_name);
}

XmlRpc::XmlRpcValue RemoveProfile(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 1)) {
    return false;
  }
  const std::string& profile_name(params_in[0]);
  return shill_wifi_client->RemoveProfile(profile_name);
}

XmlRpc::XmlRpcValue PushProfile(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 1)) {
    return false;
  }
  const std::string& profile_name(params_in[0]);
  return shill_wifi_client->PushProfile(profile_name);
}

XmlRpc::XmlRpcValue PopProfile(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 1)) {
    return false;
  }
  const std::string& profile_name(params_in[0]);
  return shill_wifi_client->PopProfile(profile_name);
}

XmlRpc::XmlRpcValue CleanProfiles(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 0)) {
    return false;
  }
  return shill_wifi_client->CleanProfiles();
}

XmlRpc::XmlRpcValue DeleteEntriesForSsid(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 1)) {
    return false;
  }
  const std::string& ssid(params_in[0]);
  return shill_wifi_client->DeleteEntriesForSsid(ssid);
}

XmlRpc::XmlRpcValue InitTestNetworkState(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 0)) {
    return false;
  }
  shill_wifi_client->CleanProfiles();
  shill_wifi_client->RemoveAllWifiEntries();
  shill_wifi_client->RemoveProfile(kTestProfileName);
  bool is_success = shill_wifi_client->CreateProfile(kTestProfileName);
  if (is_success) {
    shill_wifi_client->PushProfile(kTestProfileName);
  }
  return is_success;
}

XmlRpc::XmlRpcValue ListControlledWifiInterfaces(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 0)) {
    return false;
  }
  std::vector<std::string> interfaces;
  if (!shill_wifi_client->ListControlledWifiInterfaces(&interfaces)) {
    return false;
  }
  XmlRpc::XmlRpcValue result;
  int array_pos = 0;
  for (const auto& interface : interfaces) {
    result[array_pos++] = interface;
  }
  return result;
}

XmlRpc::XmlRpcValue Disconnect(
    XmlRpc::XmlRpcValue params_in,
    ProxyShillWifiClient* shill_wifi_client) {
  if (!ValidateNumOfElements(params_in, 1)) {
    return false;
  }
  const std::string& ssid = params_in[0];
  return shill_wifi_client->Disconnect(ssid);
}

ProxyRpcServerMethod::ProxyRpcServerMethod(
    const std::string& method_name,
    const RpcServerMethodHandler& handler,
    ProxyShillWifiClient* shill_wifi_client,
    ProxyRpcServer* server)
  : XmlRpcServerMethod(method_name, server),
    handler_(handler),
    shill_wifi_client_(shill_wifi_client) {
}

void ProxyRpcServerMethod::execute(
    XmlRpc::XmlRpcValue& params_in,
    XmlRpc::XmlRpcValue& value_out) {
  value_out = handler_.Run(params_in, shill_wifi_client_);
}

std::string ProxyRpcServerMethod::help(void) {
  // TODO: Lookup the method help using the |method_name| from
  // a text file.
  return "Shill Test Proxy RPC methods help.";
}

ProxyRpcServer::ProxyRpcServer(
    int server_port,
    std::unique_ptr<ProxyShillWifiClient> shill_wifi_client)
  : XmlRpcServer(),
    server_port_(server_port),
    shill_wifi_client_(std::move(shill_wifi_client)) {
}

void ProxyRpcServer::RegisterRpcMethod(
    const std::string& method_name,
    const RpcServerMethodHandler& handler) {
  methods_.emplace_back(
      new ProxyRpcServerMethod(
          method_name, handler, shill_wifi_client_.get(), this));
}

void ProxyRpcServer::Run() {
  XmlRpc::setVerbosity(kDefaultXmlRpcVerbosity);
  if (!XmlRpc::XmlRpcServer::bindAndListen(server_port_)) {
    LOG(ERROR) << "Failed to bind to port " << server_port_ << ".";
    return;
  }
  XmlRpc::XmlRpcServer::enableIntrospection(true);

  RegisterRpcMethod("create_profile", base::Bind(&CreateProfile));
  RegisterRpcMethod("remove_profile", base::Bind(&RemoveProfile));
  RegisterRpcMethod("push_profile", base::Bind(&PushProfile));
  RegisterRpcMethod("pop_profile", base::Bind(&PopProfile));
  RegisterRpcMethod("clean_profiles", base::Bind(&CleanProfiles));
  RegisterRpcMethod("delete_entries_for_ssid", base::Bind(&DeleteEntriesForSsid));
  RegisterRpcMethod("init_test_network_state", base::Bind(&InitTestNetworkState));
  RegisterRpcMethod("list_controlled_wifi_interfaces",
                    base::Bind(&ListControlledWifiInterfaces));
  RegisterRpcMethod("disconnect", base::Bind(&Disconnect));

  XmlRpc::XmlRpcServer::work(-1.0);
}
