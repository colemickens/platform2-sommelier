// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_daemon.h"

#include <stdio.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/file_path.h>
#include <base/logging.h>

#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/nss.h"
#include "shill/proxy_factory.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"
#include "shill/scope_logger.h"
#include "shill/shill_config.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;

namespace shill {

// TODO(gmorain): 3 seconds may or may not be enough.  Add an UMA stat to see
// how often the timeout occurs.  crosbug.com/31475.
const int Daemon::kTerminationActionsTimeout = 3000; // ms

Daemon::Daemon(Config *config, ControlInterface *control)
    : config_(config),
      control_(control),
      nss_(NSS::GetInstance()),
      proxy_factory_(ProxyFactory::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      dhcp_provider_(DHCPProvider::GetInstance()),
      manager_(new Manager(control_,
                           &dispatcher_,
                           &metrics_,
                           &glib_,
                           config->GetRunDirectory(),
                           config->GetStorageDirectory(),
                           config->GetUserStorageDirectoryFormat())) {
}
Daemon::~Daemon() {}

void Daemon::AddDeviceToBlackList(const string &device_name) {
  manager_->AddDeviceToBlackList(device_name);
}

void Daemon::SetStartupPortalList(const string &portal_list) {
  manager_->SetStartupPortalList(portal_list);
}

void Daemon::SetStartupProfiles(const vector<string> &profile_name_list) {
  Error error;
  manager_->set_startup_profiles(profile_name_list);
}

void Daemon::Run() {
  Start();
  SLOG(Daemon, 1) << "Running main loop.";
  dispatcher_.DispatchForever();
  SLOG(Daemon, 1) << "Exited main loop.";
}

void Daemon::Quit() {
  SLOG(Daemon, 1) << "Starting termination actions.";
  // Stop() prevents autoconnect from attempting to immediately connect to
  // services after they have been disconnected.
  Stop();
  manager_->RunTerminationActions(kTerminationActionsTimeout,
                                  Bind(&Daemon::TerminationActionsCompleted,
                                       Unretained(this)));
}

void Daemon::TerminationActionsCompleted(const Error & error) {
  SLOG(Daemon, 1) << "Finished termination actions.  Result: " << error;
  dispatcher_.PostTask(MessageLoop::QuitClosure());
}

void Daemon::Start() {
  glib_.TypeInit();
  nss_->Init(&glib_);
  proxy_factory_->Init();
  rtnl_handler_->Start(&dispatcher_, &sockets_);
  routing_table_->Start();
  dhcp_provider_->Init(control_, &dispatcher_, &glib_);
  manager_->Start();
}

void Daemon::Stop() {
  manager_->Stop();
}

}  // namespace shill
