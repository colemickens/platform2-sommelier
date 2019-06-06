// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/tdls_manager.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/supplicant/supplicant_interface_proxy_interface.h"
#include "shill/supplicant/wpa_supplicant.h"

using base::Bind;
using std::string;

namespace shill {

namespace log_scope {
static auto kModuleLogScope = ScopeLogger::kWiFi;
static string ObjectID(const TDLSManager* c) {
  return "(" + c->interface_name() + "-tdlsmanager)";
}
}  // namespace log_scope

const int TDLSManager::kPeerDiscoveryCleanupTimeoutSeconds = 30;

TDLSManager::TDLSManager(
    EventDispatcher* dispatcher,
    SupplicantInterfaceProxyInterface* supplicant_interface_proxy,
    const string& interface_name)
    : dispatcher_(dispatcher),
      supplicant_interface_proxy_(supplicant_interface_proxy),
      interface_name_(interface_name) {}

TDLSManager::~TDLSManager() = default;

string TDLSManager::PerformOperation(const string& peer_mac_address,
                                     const string& operation,
                                     Error* error) {
  CHECK(supplicant_interface_proxy_);

  SLOG(this, 2) << "Processing TDLS command: " << operation
                << " for peer " << peer_mac_address;

  bool success = false;
  if (operation == kTDLSDiscoverOperation) {
    success = DiscoverPeer(peer_mac_address);
  } else if (operation == kTDLSSetupOperation) {
    success = SetupPeer(peer_mac_address);
  } else if (operation == kTDLSStatusOperation) {
    string supplicant_status = PeerStatus(peer_mac_address);
    SLOG(this, 2) << "TDLS status returned: " << supplicant_status;
    if (!supplicant_status.empty()) {
      if (supplicant_status == WPASupplicant::kTDLSStateConnected) {
        return kTDLSConnectedState;
      } else if (supplicant_status == WPASupplicant::kTDLSStateDisabled) {
        return kTDLSDisabledState;
      } else if (supplicant_status ==
                 WPASupplicant::kTDLSStatePeerDoesNotExist) {
        if (CheckDiscoveryState(peer_mac_address) ==
            PeerDiscoveryState::kResponseReceived) {
          return kTDLSDisconnectedState;
        } else {
          return kTDLSNonexistentState;
        }
      } else if (supplicant_status ==
                 WPASupplicant::kTDLSStatePeerNotConnected) {
        return kTDLSDisconnectedState;
      } else {
        return kTDLSUnknownState;
      }
    }
  } else if (operation == kTDLSTeardownOperation) {
    success = TearDownPeer(peer_mac_address);
  } else {
    error->Populate(Error::kInvalidArguments, "Unknown operation");
    return "";
  }

  if (!success) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kOperationFailed,
                          "TDLS operation failed");
  }

  return "";
}

void TDLSManager::OnDiscoverResponseReceived(const string& peer_mac_address) {
  if (CheckDiscoveryState(peer_mac_address) ==
      PeerDiscoveryState::kRequestSent) {
    peer_discovery_state_[peer_mac_address] =
        PeerDiscoveryState::kResponseReceived;
  }
}

bool TDLSManager::DiscoverPeer(const string& peer_mac_address) {
  if (!supplicant_interface_proxy_->TDLSDiscover(peer_mac_address)) {
    LOG(ERROR) << "Failed to perform TDLS discover";
    return false;
  }
  peer_discovery_state_[peer_mac_address] = PeerDiscoveryState::kRequestSent;
  StartPeerDiscoveryCleanupTimer();
  return true;
}

bool TDLSManager::SetupPeer(const string& peer_mac_address) {
  if (!supplicant_interface_proxy_->TDLSSetup(peer_mac_address)) {
    LOG(ERROR) << "Failed to perform TDLS setup";
    return false;
  }
  return true;
}

bool TDLSManager::TearDownPeer(const string& peer_mac_address) {
  if (!supplicant_interface_proxy_->TDLSTeardown(peer_mac_address)) {
    LOG(ERROR) << "Failed to perform TDLS teardown";
    return false;
  }
  return true;
}

string TDLSManager::PeerStatus(const string& peer_mac_address) {
  string status;
  if (!supplicant_interface_proxy_->TDLSStatus(peer_mac_address, &status)) {
    LOG(ERROR) << "Failed to perform TDLS status";
    return "";
  }
  return status;
}

void TDLSManager::StartPeerDiscoveryCleanupTimer() {
  if (!peer_discovery_cleanup_callback_.IsCancelled()) {
    LOG(INFO) << __func__ << " TDLS cleanup timer restarted.";
  } else {
    LOG(INFO) << __func__ << " TDLS cleanup timer started.";
  }
  peer_discovery_cleanup_callback_.Reset(
      Bind(&TDLSManager::PeerDiscoveryCleanup, base::Unretained(this)));
  dispatcher_->PostDelayedTask(FROM_HERE,
                               peer_discovery_cleanup_callback_.callback(),
                               kPeerDiscoveryCleanupTimeoutSeconds * 1000);
}

void TDLSManager::PeerDiscoveryCleanup() {
  LOG(INFO) << __func__ << " TDLS peer discovery map cleared.";
  peer_discovery_state_.clear();
}

TDLSManager::PeerDiscoveryState TDLSManager::CheckDiscoveryState(
    const string& peer_mac_address) {
  auto iter = peer_discovery_state_.find(peer_mac_address);
  if (iter == peer_discovery_state_.end()) {
    return PeerDiscoveryState::kNone;
  }

  return iter->second;
}


}  // namespace shill.
