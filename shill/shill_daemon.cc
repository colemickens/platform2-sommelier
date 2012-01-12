// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_daemon.h"

#include <stdio.h>

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/logging.h>

#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/proxy_factory.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_config.h"

using std::string;
using std::vector;

namespace shill {

Daemon::Daemon(Config *config, ControlInterface *control)
    : config_(config),
      control_(control),
      manager_(control_,
               &dispatcher_,
               &metrics_,
               &glib_,
               config->GetRunDirectory(),
               config->GetStorageDirectory(),
               config->GetUserStorageDirectoryFormat()) {
}
Daemon::~Daemon() {}

void Daemon::AddDeviceToBlackList(const string &device_name) {
  manager_.AddDeviceToBlackList(device_name);
}

void Daemon::SetStartupProfiles(const vector<string> &profile_name_list) {
  Error error;
  manager_.set_startup_profiles(profile_name_list);
}

void Daemon::Run() {
  Start();
  VLOG(1) << "Running main loop.";
  dispatcher_.DispatchForever();
  VLOG(1) << "Exited main loop.";
  Stop();
}

void Daemon::Quit() {
  dispatcher_.PostTask(new MessageLoop::QuitTask);
}

void Daemon::Start() {
  glib_.TypeInit();
  ProxyFactory::GetInstance()->Init();
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);
  RoutingTable::GetInstance()->Start();
  DHCPProvider::GetInstance()->Init(control_, &dispatcher_, &glib_);
  manager_.Start();
}

void Daemon::Stop() {
  manager_.Stop();
}

}  // namespace shill
