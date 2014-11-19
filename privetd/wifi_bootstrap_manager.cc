// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>

#include "privetd/wifi_bootstrap_manager.h"

namespace privetd {

WifiBootstrapManager::WifiBootstrapManager(const std::string& state_file_path)
    : state_file_path_(state_file_path) {
}

void WifiBootstrapManager::AddStateChangeListener(const StateListener& cb) {
}

void WifiBootstrapManager::Init() {
  // TODO(wiley) Read whether we've ever been online from either shill or some
  //             state on disk.
  if (!have_ever_been_bootstrapped_) {
    // If we have no memory of being bootstrapped successfully, start in
    // bootstrapping mode.
    // TODO(wiley) Post task to start bootstrapping by entering pairing mode.
  } else {
    // TODO(wiley) Post a task to enter monitoring mode
  }
}

void WifiBootstrapManager::StartBootstrapping() {
  // Here we're basically waiting to be provided with Wifi credentials.
  state_ = kBootstrapping;
}

void WifiBootstrapManager::StartConnecting(const std::string& ssid,
                                           const std::string& passphrase) {
  VLOG(1) << "WiFi is attempting to connect. (ssid=" << ssid
          << ", pass=" << passphrase << ").";
  // Shut down the ap we've started via apmanager
  // Post a delayed task to timeout our connect attempt
  // Set ourselves up to get called back on OnApShutdown() when AP is gone.
  state_ = kConnecting;
  last_configured_ssid_ = ssid;
  // TODO(wiley) Remove this, we're just testing the general flow for now.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&WifiBootstrapManager::OnConnectSuccess,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(5));
}

void WifiBootstrapManager::StartMonitoring() {
  // Set up callbacks so that we get told when we go offline
  state_ = kMonitoring;
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

void WifiBootstrapManager::OnApShutdown() {
  // Provide credentials to shill
  // Ask shill to connect to network
  // Set ourselves up to get called back if shill succeeds.
}

void WifiBootstrapManager::OnConnectSuccess() {
  // Atomically write down that bootstrapping worked.
  VLOG(1) << "Wifi was connected successfully";
  StartMonitoring();
  setup_state_ = SetupState{SetupState::kSuccess};
}

void WifiBootstrapManager::OnConnectTimeout() {
  VLOG(1) << "Wifi timed out while connecting";
  setup_state_ = SetupState{SetupState::kError};
  // TODO(wiley) We should probably start bootstrapping again from here.
}

}  // namespace privetd
