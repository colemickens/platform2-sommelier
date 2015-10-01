
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
#include "proxy_rpc_server.h"

void ProxyRpcServer::Run() {
  // Set XML Rpc library
  XmlRpc::setVerbosity(xml_rpc_lib_verbosity_);
  // Create the server socket on the specified port
  bindAndListen(server_port_);
  // Enable introspection
  enableIntrospection(true);

  // Instantiate & Register all the supported RPC methods here
  // TODO: Need maybe some kind of factory to automatically
  // register all ProxyRpcServerMethod classes we have defined.
  ConnectWifi connect_wifi(this);
  DisconnectWifi disconnect_wifi(this);

  // Wait for requests indefinitely
  work(-1.0);
}


// MethodName: ConnectWifi
// Param1: SSID <string>
// Param2: IS_HEX_SSID <boolean>
// Param3: PSK <string>
// Return: 0 <success>, -1 <Invalid args>, 1 <Failure>
void ConnectWifi::execute(XmlRpcValue& params, XmlRpcValue& result) {
  int nArgs = params.size();
  // We expect 3 params for the connect wifi RPC command.
  if (nArgs != 3) {
    result[0] = kInvalidArgs;
    return;
  }

  // Send a message to the main proxy task to trigger the required dbus commands.
  std::string ssid = std::string(params[0]);
  bool is_hex_ssid = bool(params[1]);
  std::string psk = std::string(params[2]);

  // TODO: Use RPC Event dispatcher here
  // proxy_dbus_client_->OnConnectWifiRPCRequest(ssid, is_hex_ssid, psk);

  result[0] = kSuccess;
};

// MethodName: DisconnectWifi
// Return: 0 <success>, -1 <Invalid args>, 1 <Failure>
void DisconnectWifi::execute(XmlRpcValue& params, XmlRpcValue& result) {

  // TODO: Use RPC Event dispatcher here
  // proxy_dbus_client_->OnDisconnectWifiRPCRequest();

  result[0] = kSuccess;
};
