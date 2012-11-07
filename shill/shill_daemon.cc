// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_daemon.h"

#include <stdio.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/file_path.h>

#include "shill/callback80211_metrics.h"
#include "shill/config80211.h"
#include "shill/dhcp_provider.h"
#include "shill/diagnostics_reporter.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/nss.h"
#include "shill/proxy_factory.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_config.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;

namespace shill {

Daemon::Daemon(Config *config, ControlInterface *control)
    : config_(config),
      control_(control),
      nss_(NSS::GetInstance()),
      proxy_factory_(ProxyFactory::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      dhcp_provider_(DHCPProvider::GetInstance()),
      config80211_(Config80211::GetInstance()),
      manager_(new Manager(control_,
                           &dispatcher_,
                           &metrics_,
                           &glib_,
                           config->GetRunDirectory(),
                           config->GetStorageDirectory(),
                           config->GetUserStorageDirectoryFormat())),
      callback80211_output_(config80211_),
      callback80211_metrics_(config80211_, &metrics_) {
}

Daemon::~Daemon() { }

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
  if (!manager_->RunTerminationActionsAndNotifyMetrics(
      Bind(&Daemon::TerminationActionsCompleted, Unretained(this)),
      Metrics::kTerminationActionReasonTerminate)) {
    SLOG(Daemon, 1) << "No termination actions were run";
    dispatcher_.PostTask(MessageLoop::QuitClosure());
  }
}

void Daemon::TerminationActionsCompleted(const Error &error) {
  SLOG(Daemon, 1) << "Finished termination actions.  Result: " << error;
  metrics_.NotifyTerminationActionsCompleted(
      Metrics::kTerminationActionReasonTerminate, error.IsSuccess());
  dispatcher_.PostTask(MessageLoop::QuitClosure());
}

void Daemon::Start() {
  glib_.TypeInit();
  nss_->Init(&glib_);
  proxy_factory_->Init();
  rtnl_handler_->Start(&dispatcher_, &sockets_);
  routing_table_->Start();
  dhcp_provider_->Init(control_, &dispatcher_, &glib_);
  DiagnosticsReporter::GetInstance()->Init(&dispatcher_);

  if (config80211_) {
    config80211_->Init(&dispatcher_);
    // Subscribe to all the events in which we're interested.
    static const Config80211::EventType kEvents[] = {
      Config80211::kEventTypeConfig,
      Config80211::kEventTypeScan,
      Config80211::kEventTypeRegulatory,
      Config80211::kEventTypeMlme };

    // Install callbacks in the Config80211 singleton.
    callback80211_output_.InstallAsBroadcastCallback();
    callback80211_metrics_.InstallAsBroadcastCallback();

    for (size_t i = 0; i < arraysize(kEvents); i++) {
      config80211_->SubscribeToEvents(kEvents[i]);
    }
  }

  manager_->Start();
}

void Daemon::Stop() {
  manager_->Stop();
}

}  // namespace shill
