// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/wifi_bootstrap_manager.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/key_value_store.h>

#include "privetd/shill_client.h"

namespace privetd {

namespace {

}  // namespace

WifiBootstrapManager::WifiBootstrapManager(DaemonState* state_store,
                                           ShillClient* shill_client)
    : state_store_(state_store), shill_client_(shill_client) {
}

void WifiBootstrapManager::AddStateChangeListener(const StateListener& cb) {
}

void WifiBootstrapManager::Init() {
  chromeos::KeyValueStore state_store;
  if (!state_store_->GetBoolean(state_key::kWifiHasBeenBootstrapped,
                                &have_ever_been_bootstrapped_) ||
      !state_store_->GetString(state_key::kWifiLastConfiguredSSID,
                               &last_configured_ssid_)) {
    have_ever_been_bootstrapped_ = false;
  }
  if (!have_ever_been_bootstrapped_) {
    StartBootstrapping();
  } else {
    StartMonitoring();
  }
}

void WifiBootstrapManager::StartBootstrapping() {
  // TODO(wiley) Start up an AP with an SSID calculated from device info.
  UpdateState(kBootstrapping);
  tasks_weak_factory_.InvalidateWeakPtrs();
  if (have_ever_been_bootstrapped_) {
    on_bootstrap_timeout_task_.Reset(
        base::Bind(&WifiBootstrapManager::OnBootstrapTimeout,
                   tasks_weak_factory_.GetWeakPtr()));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, on_bootstrap_timeout_task_.callback(),
        base::TimeDelta::FromSeconds(bootstrap_timeout_seconds_));
  }
}

void WifiBootstrapManager::StartConnecting(const std::string& ssid,
                                           const std::string& passphrase) {
  VLOG(1) << "WiFi is attempting to connect. (ssid=" << ssid
          << ", pass=" << passphrase << ").";
  // TODO(wiley) Shut down the ap we've started via apmanager
  UpdateState(kConnecting);
  tasks_weak_factory_.InvalidateWeakPtrs();
  on_connect_success_task_.Reset(
      base::Bind(&WifiBootstrapManager::OnConnectSuccess,
                 tasks_weak_factory_.GetWeakPtr(), ssid));
  on_connect_timeout_task_.Reset(
      base::Bind(&WifiBootstrapManager::OnConnectTimeout,
                 tasks_weak_factory_.GetWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, on_connect_timeout_task_.callback(),
      base::TimeDelta::FromSeconds(connect_timeout_seconds_));
  shill_client_->ConnectToService(
      ssid, passphrase, on_connect_success_task_.callback(), nullptr);
}

void WifiBootstrapManager::StartMonitoring() {
  VLOG(1) << "Monitoring connectivity.";
  UpdateState(kMonitoring);
  tasks_weak_factory_.InvalidateWeakPtrs();
  // TODO(wiley) Set up callbacks so that we get told when we go offline
}

void WifiBootstrapManager::UpdateState(State new_state) {
  state_ = new_state;
  auto callback = [] (const StateListener& listener, State state) {
    listener.Run(state);
  };
  for (const StateListener& listener : state_listeners_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, listener, new_state));
  }
}

bool WifiBootstrapManager::IsRequired() const {
  return !have_ever_been_bootstrapped_;
}

ConnectionState WifiBootstrapManager::GetConnectionState() const {
  // TODO(wiley) This should really be some state that we read from shill.
  if (currently_online_) {
    return ConnectionState{ConnectionState::kOnline};
  }
  if (!have_ever_been_bootstrapped_) {
    return ConnectionState{ConnectionState::kUnconfigured};
  }
  // TODO(wiley) When we read state from shill, return some intermediate states
  //             of WiFi networks (like connecting).
  return ConnectionState{ConnectionState::kOffline};
}

SetupState WifiBootstrapManager::GetSetupState() const {
  return setup_state_;
}

bool WifiBootstrapManager::ConfigureCredentials(const std::string& ssid,
                                                const std::string& passphrase) {
  setup_state_ = SetupState{SetupState::kInProgress};
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WifiBootstrapManager::StartConnecting,
                 tasks_weak_factory_.GetWeakPtr(), ssid, passphrase));
  return true;
}

std::string WifiBootstrapManager::GetCurrentlyConnectedSsid() const {
  return last_configured_ssid_;
}

std::string WifiBootstrapManager::GetHostedSsid() const {
  // TODO(wiley) When we talk to apmanager to set up a hosted SSID, store it.
  return std::string{};
}

std::vector<WifiType> WifiBootstrapManager::GetTypes() const {
  // TODO(wiley) This should do some system work to figure this out.
  return {WifiType::kWifi24};
}

void WifiBootstrapManager::OnConnectSuccess(const std::string& ssid) {
  VLOG(1) << "Wifi was connected successfully";
  have_ever_been_bootstrapped_ = true;
  last_configured_ssid_ = ssid;
  state_store_->SetBoolean(state_key::kWifiHasBeenBootstrapped,
                           have_ever_been_bootstrapped_);
  state_store_->SetString(state_key::kWifiLastConfiguredSSID,
                          last_configured_ssid_);
  state_store_->Save();
  setup_state_ = SetupState{SetupState::kSuccess};
  StartMonitoring();
}

void WifiBootstrapManager::OnBootstrapTimeout() {
  VLOG(1) << "Bootstrapping has timed out.";
  StartMonitoring();
}

void WifiBootstrapManager::OnConnectTimeout() {
  VLOG(1) << "Wifi timed out while connecting";
  setup_state_ = SetupState{SetupState::kError};
  StartBootstrapping();
}

void WifiBootstrapManager::OnConnectivityChange(bool is_connected) {
  if (state_ != kMonitoring) {
    return;
  }
  if (is_connected) {
    on_monitoring_timeout_task_.Cancel();
  } else if (on_monitoring_timeout_task_.IsCancelled()) {
    on_monitoring_timeout_task_.Reset(
        base::Bind(&WifiBootstrapManager::OnMonitorTimeout,
                   tasks_weak_factory_.GetWeakPtr()));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, on_monitoring_timeout_task_.callback(),
        base::TimeDelta::FromSeconds(monitor_timeout_seconds_));
  }
}

void WifiBootstrapManager::OnMonitorTimeout() {
  VLOG(1) << "Spent too long offline.  Entering bootstrap mode.";
  // TODO(wiley) Retrieve relevant errors from shill.
  StartBootstrapping();
}

}  // namespace privetd
