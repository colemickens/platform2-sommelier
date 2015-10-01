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
#include "proxy_daemon.h"
#include "proxy_dbus_shill_wifi_client.h"

void ProxyDaemon::start_rpc_server_thread(std::unique_ptr<ProxyRpcServer> rpc_server) {
  rpc_server->Run();
}

int ProxyDaemon::OnInit() {
  int ret = DBusDaemon::OnInit();
  if (ret != EX_OK) {
    return ret;
  }

  // TODO: Create a RPC Event dispatcher and send it to the RPC server
  // to schedule tasks on the main thread.

  // Create the RPC server object
  rpc_server_.reset(
      new ProxyRpcServer(xml_rpc_server_port_, xml_rpc_lib_verbosity_));
  // We're creating the Dbus version of the Shill Wifi Client for now.
  shill_wifi_client_.reset(new ProxyDbusShillWifiClient(bus_));

  rpc_server_thread_ = new std::thread(
      &ProxyDaemon::start_rpc_server_thread,
      std::move(rpc_server_));
  rpc_server_thread_->detach();

  return EX_OK;
}

void ProxyDaemon::OnShutdown (int* exit_code) {
  // Todo: Kill the RPC server thread.
  return DBusDaemon::OnShutdown(exit_code);
}
