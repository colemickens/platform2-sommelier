// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_FIREWALL_MANAGER_H_
#define APMANAGER_FIREWALL_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "apmanager/firewall_proxy_interface.h"

// Class for managing required firewall rules for apmanager.
namespace apmanager {

class ControlInterface;

class FirewallManager final {
 public:
  FirewallManager();
  ~FirewallManager();

  void Init(ControlInterface* control_interface);

  // Request/release DHCP port access for the specified interface.
  void RequestDHCPPortAccess(const std::string& interface);
  void ReleaseDHCPPortAccess(const std::string& interface);

 private:
  // Invoked when remote firewall service appeared/vanished.
  void OnFirewallServiceAppeared();
  void OnFirewallServiceVanished();

  // This is called when a new instance of firewall proxy is detected. Since
  // the new instance doesn't have any knowledge of previous port access
  // requests, re-issue those requests to the proxy to get in sync.
  void RequestAllPortsAccess();

  std::unique_ptr<FirewallProxyInterface> firewall_proxy_;

  // List of interfaces with DHCP port access.
  std::set<std::string> dhcp_access_interfaces_;

  base::WeakPtrFactory<FirewallManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FirewallManager);
};

}  // namespace apmanager

#endif  // APMANAGER_FIREWALL_MANAGER_H_
