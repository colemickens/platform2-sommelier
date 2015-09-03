//
// Copyright (C) 2012 The Android Open Source Project
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

#include "shill/shill_daemon.h"

#include <string>

#include <base/bind.h>

#include "shill/dhcp/dhcp_provider.h"
#include "shill/diagnostics_reporter.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/net/ndisc.h"
#include "shill/net/rtnl_handler.h"
#include "shill/routing_table.h"
#include "shill/shill_config.h"

#if !defined(DISABLE_WIFI)
#include "shill/net/netlink_manager.h"
#include "shill/net/nl80211_message.h"
#endif  // DISABLE_WIFI

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDaemon;
static string ObjectID(Daemon* d) { return "(shill_daemon)"; }
}


Daemon::Daemon(Config* config, ControlInterface* control)
    : config_(config),
      control_(control),
      metrics_(new Metrics(&dispatcher_)),
      rtnl_handler_(RTNLHandler::GetInstance()),
      routing_table_(RoutingTable::GetInstance()),
      dhcp_provider_(DHCPProvider::GetInstance()),
#if !defined(DISABLE_WIFI)
      netlink_manager_(NetlinkManager::GetInstance()),
      callback80211_metrics_(metrics_.get()),
#endif  // DISABLE_WIFI
      manager_(new Manager(control_.get(),
                           &dispatcher_,
                           metrics_.get(),
                           &glib_,
                           config->GetRunDirectory(),
                           config->GetStorageDirectory(),
                           config->GetUserStorageDirectory())) {
}

Daemon::~Daemon() {}

void Daemon::ApplySettings(const Settings& settings) {
  for (const auto& device_name : settings.device_blacklist) {
    manager_->AddDeviceToBlackList(device_name);
  }
  Error error;
  manager_->SetTechnologyOrder(settings.default_technology_order, &error);
  CHECK(error.IsSuccess());  // Command line should have been validated.
  manager_->SetIgnoreUnknownEthernet(settings.ignore_unknown_ethernet);
  if (settings.use_portal_list) {
    manager_->SetStartupPortalList(settings.portal_list);
  }
  if (settings.passive_mode) {
    manager_->SetPassiveMode();
  }
  manager_->SetPrependDNSServers(settings.prepend_dns_servers);
  if (settings.minimum_mtu) {
    manager_->SetMinimumMTU(settings.minimum_mtu);
  }
  manager_->SetAcceptHostnameFrom(settings.accept_hostname_from);
  manager_->SetDHCPv6EnabledDevices(settings.dhcpv6_enabled_devices);
}

void Daemon::Run() {
  Start();
  SLOG(this, 1) << "Running main loop.";
  dispatcher_.DispatchForever();
  SLOG(this, 1) << "Exited main loop.";
}

void Daemon::Quit() {
  SLOG(this, 1) << "Starting termination actions.";
  if (!manager_->RunTerminationActionsAndNotifyMetrics(
          Bind(&Daemon::TerminationActionsCompleted, Unretained(this)))) {
    SLOG(this, 1) << "No termination actions were run";
    StopAndReturnToMain();
  }
}

void Daemon::TerminationActionsCompleted(const Error& error) {
  SLOG(this, 1) << "Finished termination actions.  Result: " << error;
  metrics_->NotifyTerminationActionsCompleted(error.IsSuccess());

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
  metrics_->Start();
  rtnl_handler_->Start(
      RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
      RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE | RTMGRP_ND_USEROPT);
  routing_table_->Start();
  dhcp_provider_->Init(control_.get(), &dispatcher_, &glib_, metrics_.get());

#if !defined(DISABLE_WIFI)
  if (netlink_manager_) {
    netlink_manager_->Init();
    uint16_t nl80211_family_id = netlink_manager_->GetFamily(
        Nl80211Message::kMessageTypeString,
        Bind(&Nl80211Message::CreateMessage));
    if (nl80211_family_id == NetlinkMessage::kIllegalMessageType) {
      LOG(FATAL) << "Didn't get a legal message type for 'nl80211' messages.";
    }
    Nl80211Message::SetMessageType(nl80211_family_id);
    netlink_manager_->Start();

    // Install handlers for NetlinkMessages that don't have specific handlers
    // (which are registered by message sequence number).
    netlink_manager_->AddBroadcastHandler(Bind(
        &Callback80211Metrics::CollectDisconnectStatistics,
        callback80211_metrics_.AsWeakPtr()));
  }
#endif  // DISABLE_WIFI

  manager_->Start();
}

void Daemon::Stop() {
  manager_->Stop();
  manager_ = nullptr;  // Release manager resources, including DBus adaptor.
  metrics_->Stop();
  dhcp_provider_->Stop();
}

}  // namespace shill
