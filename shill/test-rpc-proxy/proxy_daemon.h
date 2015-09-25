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

#include "proxy_dbus_client.h"
#include "proxy_rpc_server.h"

class ProxyDaemon : public chromeos::DBusDaemon {
 public:
  ProxyDaemon(ProxyDbusClient *dbus_client, ProxyRpcServer *rpc_server) :
    dbus_client_(dbus_client), rpc_server_(rpc_server) {}
  ~ProxyDaemon() override = default;
  static void start_rpc_server_thread(
      std::shared_ptr<ProxyRpcServer> rpc_server);

 protected:
  int OnInit() override;
  void OnShutdown (int* exit_code) override;

 private:
  std::shared_ptr<ProxyDbusClient> dbus_client_;
  std::shared_ptr<ProxyRpcServer> rpc_server_;
  std::thread *rpc_server_thread_;
};
#endif //PROXY_DAEMON_H
