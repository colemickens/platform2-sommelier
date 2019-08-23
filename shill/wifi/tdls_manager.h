// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_TDLS_MANAGER_H_
#define SHILL_WIFI_TDLS_MANAGER_H_

#include <map>
#include <string>

#include <base/cancelable_callback.h>

namespace shill {

class Error;
class EventDispatcher;
class SupplicantInterfaceProxyInterface;

// Manage TDLS peers for the specified interface |interface_name|.
class TDLSManager {
 public:
  TDLSManager(EventDispatcher* dispatcher,
              SupplicantInterfaceProxyInterface* supplicant_interface_proxy,
              const std::string& interface_name);
  virtual ~TDLSManager();

  // Perform TDLS |operation| on |peer|.
  virtual std::string PerformOperation(const std::string& peer_mac_address,
                                       const std::string& operation,
                                       Error* error);

  // Called when a discover response for |peer_mac_address| is received.
  virtual void OnDiscoverResponseReceived(const std::string& peer_mac_address);

  const std::string& interface_name() const { return interface_name_; }

 private:
  friend class TDLSManagerTest;

  enum class PeerDiscoveryState { kNone, kRequestSent, kResponseReceived };

  static const int kPeerDiscoveryCleanupTimeoutSeconds;

  // Discover TDLS service on a remote |peer_mac_address|.  Returns true if
  // operation is initiated successfully.
  bool DiscoverPeer(const std::string& peer_mac_address);

  // Setup a TDLS pairing with |peer_mac_address|.  Returns true if operation is
  // initiated successfully.
  bool SetupPeer(const std::string& peer_mac_address);

  // Tear down the TDLS pairing with |peer|.  Returns true if operation is
  // initiated successfully.
  bool TearDownPeer(const std::string& peer_mac_address);

  // Return a string indicating the TDLS status with |peer_mac_address|.
  std::string PeerStatus(const std::string& peer_mac_address);

  // Start the timer to delete any peer entries stored in our peer discovery
  // map.
  void StartPeerDiscoveryCleanupTimer();

  // Timeout handler to delete any peer entries from our peer discovery map.
  void PeerDiscoveryCleanup();

  // Returns the TDLS discover status for this peer
  PeerDiscoveryState CheckDiscoveryState(const std::string& peer_mac_address);

  // Executes when the TDLS peer discovery cleanup timer expires.
  base::CancelableClosure peer_discovery_cleanup_callback_;

  // Maps peer to its discovery state.
  std::map<std::string, PeerDiscoveryState> peer_discovery_state_;

  EventDispatcher* dispatcher_;
  SupplicantInterfaceProxyInterface* supplicant_interface_proxy_;
  std::string interface_name_;

  DISALLOW_COPY_AND_ASSIGN(TDLSManager);
};

}  // namespace shill

#endif  // SHILL_WIFI_TDLS_MANAGER_H_
