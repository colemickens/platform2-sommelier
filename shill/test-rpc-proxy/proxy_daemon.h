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

#ifndef PROXY_DAEMON_H
#define PROXY_DAEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <iostream>
#include <thread>

#include <base/logging.h>
#include <chromeos/any.h>
#include <chromeos/daemons/dbus_daemon.h>

#include "proxy_shill_wifi_client.h"
#include "proxy_rpc_server.h"

class ProxyDaemon : public chromeos::DBusDaemon {
 public:
  ProxyDaemon(int xml_rpc_server_port, int xml_rpc_lib_verbosity):
    xml_rpc_server_port_(xml_rpc_server_port) {}
  ~ProxyDaemon() override = default;
  static void start_rpc_server_thread(std::unique_ptr<ProxyRpcServer> rpc_server);

 protected:
  int OnInit() override;
  void OnShutdown (int* exit_code) override;

 private:
  int xml_rpc_server_port_;
  int xml_rpc_lib_verbosity_;
  std::unique_ptr<ProxyShillWifiClient> shill_wifi_client_;
  std::unique_ptr<ProxyRpcServer> rpc_server_;
  std::thread *rpc_server_thread_;

};
#endif //PROXY_DAEMON_H
