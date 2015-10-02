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
// Return values for the RPC calls.
enum RpcMethodReturnValue {
  kRpcMethodReturnInvalidArgs = -1,
  kRpcMethodReturnFailure = 0,
  kRpcMethodReturnSuccess = 1
};
// XmlRpc library verbosity level.
static const int kDefaultXmlRpcVerbosity = 5;
}

/*************** RPC Method implementations **********/
XmlRpcValue CreateProfile(XmlRpcValue params_in,
                          ProxyShillWifiClient* shill_wifi_client) {
  XmlRpcValue result;
  if (params_in.size() != 1) {
    result[0] = kRpcMethodReturnInvalidArgs;
    return result;
  }
  const std::string& profile_name(params_in[0]);
  result[0] = shill_wifi_client->CreateProfile(profile_name);
  return result;
}

XmlRpcValue RemoveProfile(XmlRpcValue params_in,
                          ProxyShillWifiClient* shill_wifi_client) {
  XmlRpcValue result;
  if (params_in.size() != 1) {
    result[0] = kRpcMethodReturnInvalidArgs;
    return result;
  }
  const std::string& profile_name(params_in[0]);
  result[0] = shill_wifi_client->RemoveProfile(profile_name);
  return result;
}

XmlRpcValue PushProfile(XmlRpcValue params_in,
                        ProxyShillWifiClient* shill_wifi_client) {
  XmlRpcValue result;
  if (params_in.size() != 1) {
    result[0] = kRpcMethodReturnInvalidArgs;
    return result;
  }
  const std::string& profile_name(params_in[0]);
  result[0] = shill_wifi_client->PushProfile(profile_name);
  return result;
}

XmlRpcValue PopProfile(XmlRpcValue params_in,
                       ProxyShillWifiClient* shill_wifi_client) {
  XmlRpcValue result;
  if (params_in.size() != 1) {
    result[0] = kRpcMethodReturnInvalidArgs;
    return result;
  }
  const std::string& profile_name(params_in[0]);
  result[0] = shill_wifi_client->PopProfile(profile_name);
  return result;
}

ProxyRpcServerMethod::ProxyRpcServerMethod(
    const std::string& method_name,
    const RpcServerMethodHandler& handler,
    ProxyShillWifiClient* shill_wifi_client,
    XmlRpcServer* server)
  : XmlRpcServerMethod(method_name, server),
    handler_(handler),
    shill_wifi_client_(shill_wifi_client) {
}

void ProxyRpcServerMethod::execute(
    XmlRpcValue& params_in,
    XmlRpcValue& value_out) {
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
  if (!XmlRpcServer::bindAndListen(server_port_)) {
    LOG(ERROR) << "Failed to bind to port " << server_port_ << ".";
    return;
  }
  XmlRpcServer::enableIntrospection(true);

  RegisterRpcMethod("create_profile", base::Bind(&CreateProfile));
  RegisterRpcMethod("remove_profile", base::Bind(&RemoveProfile));
  RegisterRpcMethod("push_profile", base::Bind(&PushProfile));
  RegisterRpcMethod("pop_profile", base::Bind(&PopProfile));

  XmlRpcServer::work(-1.0);
}
