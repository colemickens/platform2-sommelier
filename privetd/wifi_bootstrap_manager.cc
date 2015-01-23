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

#include "privetd/ap_manager_client.h"
#include "privetd/shill_client.h"

namespace privetd {

WifiBootstrapManager::WifiBootstrapManager(DaemonState* state_store,
                                           ShillClient* shill_client,
                                           ApManagerClient* ap_manager_client,
                                           const DeviceDelegate* device,
                                           const CloudDelegate* gcd,
                                           uint32_t connect_timeout_seconds,
                                           uint32_t bootstrap_timeout_seconds,
                                           uint32_t monitor_timeout_seconds)
    : state_store_(state_store),
      shill_client_(shill_client),
      ap_manager_client_(ap_manager_client),
      ssid_generator_(device, gcd, this),
      connect_timeout_seconds_(connect_timeout_seconds),
      bootstrap_timeout_seconds_(bootstrap_timeout_seconds),
      monitor_timeout_seconds_(monitor_timeout_seconds) {
}

void WifiBootstrapManager::Init() {
  chromeos::KeyValueStore state_store;
  if (!state_store_->GetBoolean(state_key::kWifiHasBeenBootstrapped,
                                &have_ever_been_bootstrapped_) ||
      !state_store_->GetString(state_key::kWifiLastConfiguredSSID,
                               &last_configured_ssid_)) {
    have_ever_been_bootstrapped_ = false;
  }
  shill_client_->RegisterConnectivityListener(
      base::Bind(&WifiBootstrapManager::OnConnectivityChange,
                 lifetime_weak_factory_.GetWeakPtr()));
  if (!have_ever_been_bootstrapped_) {
    StartBootstrapping();
  } else {
    StartMonitoring();
  }
}

void WifiBootstrapManager::RegisterStateListener(
    const StateListener& listener) {
  state_listeners_.push_back(listener);
}

void WifiBootstrapManager::StartBootstrapping() {
  UpdateState(kBootstrapping);
  if (have_ever_been_bootstrapped_) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&WifiBootstrapManager::OnBootstrapTimeout,
                              tasks_weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(bootstrap_timeout_seconds_));
  }
  // TODO(vitalybuka): Add SSID probing.
  ap_manager_client_->Start(ssid_generator_.GenerateSsid());
}

void WifiBootstrapManager::EndBootstrapping() {
  ap_manager_client_->Stop();
}

void WifiBootstrapManager::StartConnecting(const std::string& ssid,
                                           const std::string& passphrase) {
  VLOG(1) << "WiFi is attempting to connect. (ssid=" << ssid
          << ", pass=" << passphrase << ").";
  // TODO(wiley) Shut down the ap we've started via apmanager
  UpdateState(kConnecting);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&WifiBootstrapManager::OnConnectTimeout,
                            tasks_weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(connect_timeout_seconds_));
  shill_client_->ConnectToService(
      ssid, passphrase, base::Bind(&WifiBootstrapManager::OnConnectSuccess,
                                   tasks_weak_factory_.GetWeakPtr(), ssid),
      nullptr);
}

void WifiBootstrapManager::EndConnecting() {
}

void WifiBootstrapManager::StartMonitoring() {
  VLOG(1) << "Monitoring connectivity.";
  UpdateState(kMonitoring);
  // TODO(wiley) Set up callbacks so that we get told when we go offline
}

void WifiBootstrapManager::EndMonitoring() {
}

void WifiBootstrapManager::UpdateState(State new_state) {
  VLOG(3) << "Switching state from " << state_ << " to " << new_state;
  // Abort irrelevant tasks.
  tasks_weak_factory_.InvalidateWeakPtrs();

  switch (state_) {
    case kDisabled:
      break;
    case kBootstrapping:
      EndBootstrapping();
      break;
    case kMonitoring:
      EndMonitoring();
      break;
    case kConnecting:
      EndConnecting();
      break;
  }

  state_ = new_state;

  // Post with weak ptr to avoid notification after this object destroyed.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&WifiBootstrapManager::NotifyStateListeners,
                            lifetime_weak_factory_.GetWeakPtr(), new_state));
}

void WifiBootstrapManager::NotifyStateListeners(State new_state) const {
  for (const StateListener& listener : state_listeners_)
    listener.Run(new_state);
}

bool WifiBootstrapManager::IsRequired() const {
  return !have_ever_been_bootstrapped_;
}

ConnectionState WifiBootstrapManager::GetConnectionState() const {
  if (!have_ever_been_bootstrapped_) {
    return ConnectionState{ConnectionState::kUnconfigured};
  }
  ShillClient::ServiceState service_state{shill_client_->GetConnectionState()};
  switch (service_state) {
    case ShillClient::kOffline:
      return ConnectionState{ConnectionState::kOffline};
    case ShillClient::kFailure:
      // TODO(wiley) Pull error information from somewhere.
      return ConnectionState{ConnectionState::kError};
    case ShillClient::kConnecting:
      return ConnectionState{ConnectionState::kConnecting};
    case ShillClient::kConnected:
      return ConnectionState{ConnectionState::kOnline};
    default:
      LOG(WARNING) << "Unknown state returned from ShillClient: "
                   << service_state;
      break;
  }
  return ConnectionState{Error::kDeviceConfigError};
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
  // TODO(vitalybuka): Get from shill, if possible.
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
  VLOG(3) << "ConnectivityChanged: " << is_connected;
  if (state_ != kMonitoring) {
    return;
  }
  if (is_connected) {
    tasks_weak_factory_.InvalidateWeakPtrs();
  } else {
    // Tasks queue may have more than one OnMonitorTimeout enqueued. The first
    // one could be executed as it would change the state and abort the rest.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&WifiBootstrapManager::OnMonitorTimeout,
                              tasks_weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(monitor_timeout_seconds_));
  }
}

void WifiBootstrapManager::OnMonitorTimeout() {
  VLOG(1) << "Spent too long offline.  Entering bootstrap mode.";
  // TODO(wiley) Retrieve relevant errors from shill.
  StartBootstrapping();
}

}  // namespace privetd
