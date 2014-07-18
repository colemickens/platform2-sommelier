// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_daemon.h"

#include <string>

#include <base/bind.h>

#include "shill/callback80211_metrics.h"
#include "shill/dhcp_provider.h"
#include "shill/diagnostics_reporter.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/netlink_manager.h"
#include "shill/nl80211_message.h"
#include "shill/proxy_factory.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_config.h"

using base::Bind;
using base::Unretained;
using std::string;

namespace shill {

Daemon::Daemon(Config *config, ControlInterface *control)
    : config_(config),
      control_(control),
      metrics_(new Metrics(&dispatcher_)),
      proxy_factory_(ProxyFactory::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      dhcp_provider_(DHCPProvider::GetInstance()),
      netlink_manager_(NetlinkManager::GetInstance()),
      manager_(new Manager(control_,
                           &dispatcher_,
                           metrics_.get(),
                           &glib_,
                           config->GetRunDirectory(),
                           config->GetStorageDirectory(),
                           config->GetUserStorageDirectory())),
      callback80211_metrics_(metrics_.get()) {
}

Daemon::~Daemon() {}

void Daemon::AddDeviceToBlackList(const string &device_name) {
  manager_->AddDeviceToBlackList(device_name);
}

void Daemon::SetStartupPortalList(const string &portal_list) {
  manager_->SetStartupPortalList(portal_list);
}

void Daemon::Run() {
  Start();
  SLOG(Daemon, 1) << "Running main loop.";
  dispatcher_.DispatchForever();
  SLOG(Daemon, 1) << "Exited main loop.";
}

void Daemon::Quit() {
  SLOG(Daemon, 1) << "Starting termination actions.";
  if (!manager_->RunTerminationActionsAndNotifyMetrics(
          Bind(&Daemon::TerminationActionsCompleted, Unretained(this)),
          Metrics::kTerminationActionReasonTerminate)) {
    SLOG(Daemon, 1) << "No termination actions were run";
    StopAndReturnToMain();
  }
}

void Daemon::TerminationActionsCompleted(const Error &error) {
  SLOG(Daemon, 1) << "Finished termination actions.  Result: " << error;
  metrics_->NotifyTerminationActionsCompleted(
      Metrics::kTerminationActionReasonTerminate, error.IsSuccess());

  // Daemon::TerminationActionsCompleted() should not directly call
  // Daemon::Stop(). Otherwise, it could lead to the call sequence below. That
  // is not safe as the HookTable's start callback only holds a weak pointer to
  // the Cellular object, which is destroyed in midst of the
  // Cellular::OnTerminationCompleted() call. We schedule the
  // Daemon::StopAndReturnToMain() call through the message loop instead.
  //
  // Daemon::Quit
  //   -> Manager::RunTerminationActionsAndNotifyMetrics
  //     -> Manager::RunTerminationActions
  //       -> HookTable::Run
  //         ...
  //         -> Cellular::OnTerminationCompleted
  //           -> Manager::TerminationActionComplete
  //             -> HookTable::ActionComplete
  //               -> Daemon::TerminationActionsCompleted
  //                 -> Daemon::Stop
  //                   -> Manager::Stop
  //                     -> DeviceInfo::Stop
  //                       -> Cellular::~Cellular
  //           -> Manager::RemoveTerminationAction
  dispatcher_.PostTask(Bind(&Daemon::StopAndReturnToMain, Unretained(this)));
}

void Daemon::StopAndReturnToMain() {
  Stop();
  dispatcher_.PostTask(base::MessageLoop::QuitClosure());
}

void Daemon::Start() {
  glib_.TypeInit();
  proxy_factory_->Init();
  metrics_->Start();
  rtnl_handler_->Start(&dispatcher_, &sockets_);
  routing_table_->Start();
  dhcp_provider_->Init(control_, &dispatcher_, &glib_, metrics_.get());

  if (netlink_manager_) {
    netlink_manager_->Init();
    uint16_t nl80211_family_id = netlink_manager_->GetFamily(
        Nl80211Message::kMessageTypeString,
        Bind(&Nl80211Message::CreateMessage));
    if (nl80211_family_id == NetlinkMessage::kIllegalMessageType) {
      LOG(FATAL) << "Didn't get a legal message type for 'nl80211' messages.";
    }
    Nl80211Message::SetMessageType(nl80211_family_id);
    netlink_manager_->Start(&dispatcher_);

    // Install handlers for NetlinkMessages that don't have specific handlers
    // (which are registered by message sequence number).
    netlink_manager_->AddBroadcastHandler(Bind(
        &Callback80211Metrics::CollectDisconnectStatistics,
        callback80211_metrics_.AsWeakPtr()));
  }

  manager_->Start();
}

void Daemon::Stop() {
  manager_->Stop();
  metrics_->Stop();
}

}  // namespace shill
