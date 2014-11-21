// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/key_value_store.h>

#include "privetd/wifi_bootstrap_manager.h"

namespace privetd {

namespace {

const char kHaveBeenBootstrappedStateKey[] = "have_ever_been_bootstrapped";
const char kLastConfiguredSSIDStateKey[] = "last_configured_ssid";

}  // namespace

WifiBootstrapManager::WifiBootstrapManager(
    const base::FilePath& state_file_path) : state_file_path_(state_file_path) {
}

void WifiBootstrapManager::AddStateChangeListener(const StateListener& cb) {
}

void WifiBootstrapManager::Init() {
  chromeos::KeyValueStore state_store;
  state_store.Load(state_file_path_);
  if (!state_store.GetBoolean(kHaveBeenBootstrappedStateKey,
                              &have_ever_been_bootstrapped_) ||
      !state_store.GetString(kLastConfiguredSSIDStateKey,
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
  if (have_ever_been_bootstrapped_) {
    on_bootstrap_timeout_task_.Reset(
        base::Bind(&WifiBootstrapManager::OnBootstrapTimeout,
                   weak_factory_.GetWeakPtr()));
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
  // TODO(wiley) Provide credentials to shill
  // TODO(wiley) Ask shill to connect to network
  // TODO(wiley) Set ourselves up to get called back if shill succeeds.
  UpdateState(kConnecting);
  on_bootstrap_timeout_task_.Cancel();
  on_connect_success_task_.Reset(
      base::Bind(&WifiBootstrapManager::OnConnectSuccess,
                 weak_factory_.GetWeakPtr(), ssid));
  on_connect_timeout_task_.Reset(
      base::Bind(&WifiBootstrapManager::OnConnectTimeout,
                 weak_factory_.GetWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, on_connect_timeout_task_.callback(),
      base::TimeDelta::FromSeconds(connect_timeout_seconds_));
  // TODO(wiley) Remove this, we're just testing the general flow for now.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, on_connect_success_task_.callback(),
      base::TimeDelta::FromSeconds(5));
}

void WifiBootstrapManager::StartMonitoring() {
  UpdateState(kMonitoring);
  on_bootstrap_timeout_task_.Cancel();
  on_connect_success_task_.Cancel();
  on_connect_timeout_task_.Cancel();
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
                 weak_factory_.GetWeakPtr(), ssid, passphrase));
  return true;
}

std::string WifiBootstrapManager::GetCurrentlyConnectedSsid() const {
  // TODO(wiley) Ask shill.
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
  on_connect_timeout_task_.Cancel();
  have_ever_been_bootstrapped_ = true;
  last_configured_ssid_ = ssid;
  chromeos::KeyValueStore state_store;
  state_store.Load(state_file_path_);
  state_store.SetBoolean(kHaveBeenBootstrappedStateKey,
                         have_ever_been_bootstrapped_);
  state_store.SetString(kLastConfiguredSSIDStateKey, last_configured_ssid_);
  state_store.Save(state_file_path_);
  StartMonitoring();
  setup_state_ = SetupState{SetupState::kSuccess};
}

void WifiBootstrapManager::OnBootstrapTimeout() {
  VLOG(1) << "Bootstrapping has timed out.";
  StartMonitoring();
}

void WifiBootstrapManager::OnConnectTimeout() {
  VLOG(1) << "Wifi timed out while connecting";
  on_connect_success_task_.Cancel();
  setup_state_ = SetupState{SetupState::kError};
  StartBootstrapping();
}

}  // namespace privetd
