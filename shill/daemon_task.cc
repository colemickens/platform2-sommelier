// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/daemon_task.h"

#include <base/bind.h>

#include "shill/control_interface.h"
#include "shill/dbus/chromeos_dbus_control.h"
#include "shill/dhcp/dhcp_provider.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/net/ndisc.h"
#include "shill/net/rtnl_handler.h"
#include "shill/process_manager.h"
#include "shill/routing_table.h"
#include "shill/shill_config.h"

#if !defined(DISABLE_WIFI)
#include "shill/net/netlink_manager.h"
#include "shill/net/nl80211_message.h"
#endif  // DISABLE_WIFI

using base::Bind;
using base::Unretained;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDaemon;
static string ObjectID(DaemonTask* d) { return "(chromeos_daemon)"; }
}

DaemonTask::DaemonTask(const Settings& settings, Config* config)
    : settings_(settings),
      config_(config),
      rtnl_handler_(nullptr),
      routing_table_(nullptr),
      dhcp_provider_(nullptr),
#if !defined(DISABLE_WIFI)
      netlink_manager_(nullptr),
#endif  // DISABLE_WIFI
      process_manager_(nullptr) {
}

DaemonTask::~DaemonTask() {}

void DaemonTask::ApplySettings() {
  manager_->SetBlacklistedDevices(settings_.device_blacklist);
  manager_->SetWhitelistedDevices(settings_.device_whitelist);
  manager_->SetArcDevice(settings_.arc_device);
  Error error;
  manager_->SetTechnologyOrder(settings_.default_technology_order, &error);
  CHECK(error.IsSuccess());  // Command line should have been validated.
  manager_->SetIgnoreUnknownEthernet(settings_.ignore_unknown_ethernet);
  if (settings_.use_portal_list) {
    manager_->SetStartupPortalList(settings_.portal_list);
  }
  if (settings_.passive_mode) {
    manager_->SetPassiveMode();
  }
  manager_->SetPrependDNSServers(settings_.prepend_dns_servers);
  if (settings_.minimum_mtu) {
    manager_->SetMinimumMTU(settings_.minimum_mtu);
  }
  manager_->SetAcceptHostnameFrom(settings_.accept_hostname_from);
  manager_->SetDHCPv6EnabledDevices(settings_.dhcpv6_enabled_devices);
  manager_->SetJailVpnClients(settings_.jail_vpn_clients);
}

bool DaemonTask::Quit(const base::Closure& completion_callback) {
  SLOG(this, 1) << "Starting termination actions.";
  if (manager_->RunTerminationActionsAndNotifyMetrics(
          Bind(&DaemonTask::TerminationActionsCompleted, Unretained(this)))) {
    SLOG(this, 1) << "Will wait for termination actions to complete";
    termination_completed_callback_ = completion_callback;
    return false;  // Note to caller: don't exit yet!
  } else {
    SLOG(this, 1) << "No termination actions were run";
    StopAndReturnToMain();
    return true;  // All done, ready to exit.
  }
}

void DaemonTask::Init() {
  dispatcher_.reset(new EventDispatcher());
  control_.reset(new ChromeosDBusControl(dispatcher_.get()));
  metrics_.reset(new Metrics(dispatcher_.get()));
  rtnl_handler_ = RTNLHandler::GetInstance();
  routing_table_ = RoutingTable::GetInstance();
  dhcp_provider_ = DHCPProvider::GetInstance();
  process_manager_ = ProcessManager::GetInstance();
#if !defined(DISABLE_WIFI)
  netlink_manager_ = NetlinkManager::GetInstance();
  callback80211_metrics_.reset(new Callback80211Metrics(metrics_.get()));
#endif  // DISABLE_WIFI
  manager_.reset(new Manager(control_.get(), dispatcher_.get(), metrics_.get(),
                             config_->GetRunDirectory(),
                             config_->GetStorageDirectory(),
                             config_->GetUserStorageDirectory()));
  control_->RegisterManagerObject(
      manager_.get(), base::Bind(&DaemonTask::Start, base::Unretained(this)));
  ApplySettings();
}

void DaemonTask::TerminationActionsCompleted(const Error& error) {
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
  dispatcher_->PostTask(FROM_HERE,
      Bind(&DaemonTask::StopAndReturnToMain, Unretained(this)));
}

void DaemonTask::StopAndReturnToMain() {
  Stop();
  if (!termination_completed_callback_.is_null()) {
    termination_completed_callback_.Run();
  }
}

void DaemonTask::Start() {
  metrics_->Start();
  rtnl_handler_->Start(RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE |
                       RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE |
                       RTMGRP_ND_USEROPT);
  routing_table_->Start();
  dhcp_provider_->Init(control_.get(), dispatcher_.get(), metrics_.get());
  process_manager_->Init(dispatcher_.get());
#if !defined(DISABLE_WIFI)
  if (netlink_manager_) {
    netlink_manager_->Init();
    uint16_t nl80211_family_id =
        netlink_manager_->GetFamily(Nl80211Message::kMessageTypeString,
                                    Bind(&Nl80211Message::CreateMessage));
    if (nl80211_family_id == NetlinkMessage::kIllegalMessageType) {
      LOG(FATAL) << "Didn't get a legal message type for 'nl80211' messages.";
    }
    Nl80211Message::SetMessageType(nl80211_family_id);
    netlink_manager_->Start();

    // Install handlers for NetlinkMessages that don't have specific handlers
    // (which are registered by message sequence number).
    netlink_manager_->AddBroadcastHandler(
        Bind(&Callback80211Metrics::CollectDisconnectStatistics,
             callback80211_metrics_->AsWeakPtr()));
  }
#endif  // DISABLE_WIFI

  manager_->Start();
}

void DaemonTask::Stop() {
  manager_->Stop();
  manager_ = nullptr;  // Release manager resources, including DBus adaptor.
#if !defined(DISABLE_WIFI)
  callback80211_metrics_ = nullptr;
#endif  // DISABLE_WIFI
  metrics_->Stop();
  dhcp_provider_->Stop();
  process_manager_->Stop();
  metrics_ = nullptr;
  // Must retain |control_|, as the D-Bus library may
  // have some work left to do. See crbug.com/537771.
}

void DaemonTask::BreakTerminationLoop() {
  // Break out of the termination loop, to continue on with other shutdown
  // tasks.
  brillo::MessageLoop::current()->BreakLoop();
}

}  // namespace shill
