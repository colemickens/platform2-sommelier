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
#ifndef PROXY_RPC_SERVER_H
#define PROXY_RPC_SERVER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <iostream>

#include <base/logging.h>

#include "XmlRpc.h"

#include "proxy_dbus_client.h"

using namespace XmlRpc;

class ProxyRpcServer : public XmlRpcServer {
 public:
  ProxyRpcServer(
      int server_port,
      int xml_rpc_lib_verbosity) :
    XmlRpcServer(),
    server_port_(server_port),
    xml_rpc_lib_verbosity_(xml_rpc_lib_verbosity) {}

  void Run();

  void set_proxy_dbus_client(std::shared_ptr<ProxyDbusClient> dbus_client) {
    proxy_dbus_client_ = dbus_client;
  }

  std::shared_ptr<ProxyDbusClient> get_proxy_dbus_client() {
    return proxy_dbus_client_;
  }

 private:
  int server_port_;
  int xml_rpc_lib_verbosity_;
  std::shared_ptr<ProxyDbusClient> proxy_dbus_client_;
};

// Generic class for all the RPC methods exposed by Shill RPC server
class ProxyRpcServerMethod : public XmlRpcServerMethod {

 public:
   ProxyRpcServerMethod(ProxyRpcServer* rpc_server) :
     XmlRpcServerMethod(typeid(*this).name(), rpc_server),
     proxy_dbus_client_(rpc_server->get_proxy_dbus_client()) {}

 protected:
   int kSuccess = 0;
   int kFailure = 1;
   int kInvalidArgs = -1;
   std::shared_ptr<ProxyDbusClient> proxy_dbus_client_;

 private:
};

// MethodName: ConnectWifi
// Param1: SSID <string>
// Param2: IS_HEX_SSID <boolean>
// Param3: PSK <string>
// Return: 0 <success>, -1 <Invalid args>, 1 <Failure>
class ConnectWifi : public ProxyRpcServerMethod {
  using ProxyRpcServerMethod::ProxyRpcServerMethod;

  void execute(XmlRpcValue& params, XmlRpcValue& result);
};

// MethodName: DisconnectWifi
// Return: 0 <success>, -1 <Invalid args>, 1 <Failure>
class DisconnectWifi : public ProxyRpcServerMethod {
  using ProxyRpcServerMethod::ProxyRpcServerMethod;

  void execute(XmlRpcValue& params, XmlRpcValue& result);
};

#endif //PROXY_RPC_SERVER_H
