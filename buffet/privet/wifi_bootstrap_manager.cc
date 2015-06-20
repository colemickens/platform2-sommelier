// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/wifi_bootstrap_manager.h"

#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/key_value_store.h>

#include "buffet/privet/ap_manager_client.h"
#include "buffet/privet/constants.h"
#include "buffet/privet/shill_client.h"

namespace privetd {

namespace {
const int kConnectTimeoutSeconds = 60;
const int kBootstrapTimeoutSeconds = 600;
const int kMonitorTimeoutSeconds = 120;
}

WifiBootstrapManager::WifiBootstrapManager(DaemonState* state_store,
                                           ShillClient* shill_client,
                                           ApManagerClient* ap_manager_client,
                                           CloudDelegate* gcd)
    : state_store_(state_store),
      shill_client_(shill_client),
      ap_manager_client_(ap_manager_client),
      ssid_generator_(gcd, this) {
  cloud_observer_.Add(gcd);
}

void WifiBootstrapManager::Init() {
  CHECK(!is_initialized_);
  std::string ssid = ssid_generator_.GenerateSsid();
  if (ssid.empty())
    return;  // Delay initialization until ssid_generator_ is ready.
  chromeos::KeyValueStore state_store;
  if (!state_store_->GetBoolean(state_key::kWifiHasBeenBootstrapped,
                                &have_ever_been_bootstrapped_) ||
      !state_store_->GetString(state_key::kWifiLastConfiguredSSID,
                               &last_configured_ssid_)) {
    have_ever_been_bootstrapped_ = false;
  }
  UpdateConnectionState();
  shill_client_->RegisterConnectivityListener(
      base::Bind(&WifiBootstrapManager::OnConnectivityChange,
                 lifetime_weak_factory_.GetWeakPtr()));
  if (!have_ever_been_bootstrapped_) {
    StartBootstrapping();
  } else {
    StartMonitoring();
  }
  is_initialized_ = true;
}

void WifiBootstrapManager::RegisterStateListener(
    const StateListener& listener) {
  // Notify about current state.
  listener.Run(state_);
  state_listeners_.push_back(listener);
}

void WifiBootstrapManager::StartBootstrapping() {
  if (shill_client_->AmOnline()) {
    // If one of the devices we monitor for connectivity is online, we need not
    // start an AP.  For most devices, this is a situation which happens in
    // testing when we have an ethernet connection.  If you need to always
    // start an AP to bootstrap WiFi credentials, then add your WiFi interface
    // to the device whitelist.
    StartMonitoring();
    return;
  }

  UpdateState(kBootstrapping);
  if (have_ever_been_bootstrapped_) {
    // If we have been configured before, we'd like to periodically take down
    // our AP and find out if we can connect again.  Many kinds of failures are
    // transient, and having an AP up prohibits us from connecting as a client.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&WifiBootstrapManager::OnBootstrapTimeout,
                              tasks_weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kBootstrapTimeoutSeconds));
  }
  // TODO(vitalybuka): Add SSID probing.
  std::string ssid = ssid_generator_.GenerateSsid();
  CHECK(!ssid.empty());
  ap_manager_client_->Start(ssid);
}

void WifiBootstrapManager::EndBootstrapping() {
  ap_manager_client_->Stop();
}

void WifiBootstrapManager::StartConnecting(const std::string& ssid,
                                           const std::string& passphrase) {
  VLOG(1) << "WiFi is attempting to connect. (ssid=" << ssid
          << ", pass=" << passphrase << ").";
  UpdateState(kConnecting);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&WifiBootstrapManager::OnConnectTimeout,
                            tasks_weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kConnectTimeoutSeconds));
  shill_client_->ConnectToService(
      ssid, passphrase, base::Bind(&WifiBootstrapManager::OnConnectSuccess,
                                   tasks_weak_factory_.GetWeakPtr(), ssid),
      nullptr);
}

void WifiBootstrapManager::EndConnecting() {
}

void WifiBootstrapManager::StartMonitoring() {
  VLOG(1) << "Monitoring connectivity.";
  // We already have a callback in place with |shill_client_| to update our
  // connectivity state.  See OnConnectivityChange().
  UpdateState(kMonitoring);
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

  if (new_state != state_) {
    state_ = new_state;
    // Post with weak ptr to avoid notification after this object destroyed.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&WifiBootstrapManager::NotifyStateListeners,
                              lifetime_weak_factory_.GetWeakPtr(), new_state));
  } else {
    VLOG(3) << "Not notifying listeners of state change, "
            << "because the states are the same.";
  }
}

void WifiBootstrapManager::NotifyStateListeners(State new_state) const {
  for (const StateListener& listener : state_listeners_)
    listener.Run(new_state);
}

const ConnectionState& WifiBootstrapManager::GetConnectionState() const {
  return connection_state_;
}

const SetupState& WifiBootstrapManager::GetSetupState() const {
  return setup_state_;
}

bool WifiBootstrapManager::ConfigureCredentials(const std::string& ssid,
                                                const std::string& passphrase,
                                                chromeos::ErrorPtr* error) {
  setup_state_ = SetupState{SetupState::kInProgress};
  // TODO(vitalybuka): Find more reliable way to finish request or move delay
  // into PrivetHandler as it's very HTTP specific.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&WifiBootstrapManager::StartConnecting,
                            tasks_weak_factory_.GetWeakPtr(), ssid, passphrase),
      base::TimeDelta::FromSeconds(kSetupDelaySeconds));
  return true;
}

std::string WifiBootstrapManager::GetCurrentlyConnectedSsid() const {
  // TODO(vitalybuka): Get from shill, if possible.
  return last_configured_ssid_;
}

std::string WifiBootstrapManager::GetHostedSsid() const {
  return ap_manager_client_->GetSsid();
}

std::set<WifiType> WifiBootstrapManager::GetTypes() const {
  // TODO(wiley) This should do some system work to figure this out.
  return {WifiType::kWifi24};
}

void WifiBootstrapManager::OnDeviceInfoChanged() {
  // Initialization was delayed until buffet is ready.
  if (!is_initialized_)
    Init();
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
  chromeos::ErrorPtr error;
  chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                         errors::kInvalidState,
                         "Failed to connect to provided network");
  setup_state_ = SetupState{std::move(error)};
  StartBootstrapping();
}

void WifiBootstrapManager::OnConnectivityChange(bool is_connected) {
  VLOG(3) << "ConnectivityChanged: " << is_connected;
  UpdateConnectionState();

  if (state_ == kBootstrapping) {
    StartMonitoring();
    return;
  }
  if (state_ == kMonitoring) {
    if (is_connected) {
      tasks_weak_factory_.InvalidateWeakPtrs();
    } else {
      // Tasks queue may have more than one OnMonitorTimeout enqueued. The
      // first one could be executed as it would change the state and abort the
      // rest.
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE, base::Bind(&WifiBootstrapManager::OnMonitorTimeout,
                                tasks_weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kMonitorTimeoutSeconds));
    }
  }
}

void WifiBootstrapManager::OnMonitorTimeout() {
  VLOG(1) << "Spent too long offline.  Entering bootstrap mode.";
  // TODO(wiley) Retrieve relevant errors from shill.
  StartBootstrapping();
}

void WifiBootstrapManager::UpdateConnectionState() {
  connection_state_ = ConnectionState{ConnectionState::kUnconfigured};
  if (!have_ever_been_bootstrapped_) {
    return;
  }
  ServiceState service_state{shill_client_->GetConnectionState()};
  switch (service_state) {
    case ServiceState::kOffline:
      connection_state_ = ConnectionState{ConnectionState::kOffline};
      return;
    case ServiceState::kFailure: {
      // TODO(wiley) Pull error information from somewhere.
      chromeos::ErrorPtr error;
      chromeos::Error::AddTo(&error, FROM_HERE, errors::kDomain,
                             errors::kInvalidState, "Unknown WiFi error");
      connection_state_ = ConnectionState{std::move(error)};
      return;
    }
    case ServiceState::kConnecting:
      connection_state_ = ConnectionState{ConnectionState::kConnecting};
      return;
    case ServiceState::kConnected:
      connection_state_ = ConnectionState{ConnectionState::kOnline};
      return;
  }
  chromeos::ErrorPtr error;
  chromeos::Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                               errors::kInvalidState,
                               "Unknown state returned from ShillClient: %s",
                               ServiceStateToString(service_state).c_str());
  connection_state_ = ConnectionState{std::move(error)};
}

}  // namespace privetd
