// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_daemon.h"

#include <stdio.h>

#include <string>

#include <base/file_path.h>
#include <base/logging.h>

#include "shill/dhcp_provider.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_config.h"

using std::string;

namespace shill {

// Daemon: Main for connection manager.  Starts main process and holds event
// loop.

Daemon::Daemon(Config *config, ControlInterface *control)
    : config_(config),
      control_(control),
      manager_(control_,
               &dispatcher_,
               &glib_,
               config->RunDirectory(),
               config->StorageDirectory(),
               config->UserStorageDirectoryFormat()) {
}
Daemon::~Daemon() {}

void Daemon::AddDeviceToBlackList(const string &device_name) {
  manager_.AddDeviceToBlackList(device_name);
}

void Daemon::Start() {
  glib_.TypeInit();
  ProxyFactory::GetInstance()->Init();
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);
  DHCPProvider::GetInstance()->Init(control_, &dispatcher_, &glib_);
  manager_.Start();
}

void Daemon::Run() {
  Start();
  VLOG(1) << "Running main loop.";
  dispatcher_.DispatchForever();
}


}  // namespace shill
