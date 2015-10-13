// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/firewall_manager.h"

#include <base/bind.h>
#include <brillo/errors/error.h>

#include "apmanager/control_interface.h"

using std::string;

namespace apmanager {

namespace {
const uint16_t kDhcpServerPort = 67;
}  // namespace

FirewallManager::FirewallManager() {}

FirewallManager::~FirewallManager() {}

void FirewallManager::Init(ControlInterface* control_interface) {
  CHECK(!firewall_proxy_) << "Already started";
  firewall_proxy_ =
      control_interface->CreateFirewallProxy(
          base::Bind(&FirewallManager::OnFirewallServiceAppeared,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&FirewallManager::OnFirewallServiceVanished,
                     weak_factory_.GetWeakPtr()));
}

void FirewallManager::RequestDHCPPortAccess(const std::string& interface) {
  CHECK(firewall_proxy_) << "Proxy not initialized yet";
  if (dhcp_access_interfaces_.find(interface) !=
      dhcp_access_interfaces_.end()) {
    LOG(ERROR) << "DHCP access already requested for interface: " << interface;
    return;
  }
  firewall_proxy_->RequestUdpPortAccess(interface, kDhcpServerPort);
  dhcp_access_interfaces_.insert(interface);
}

void FirewallManager::ReleaseDHCPPortAccess(const std::string& interface) {
  CHECK(firewall_proxy_) << "Proxy not initialized yet";
  if (dhcp_access_interfaces_.find(interface) ==
      dhcp_access_interfaces_.end()) {
    LOG(ERROR) << "DHCP access has not been requested for interface: "
               << interface;
    return;
  }
  firewall_proxy_->ReleaseUdpPortAccess(interface, kDhcpServerPort);
  dhcp_access_interfaces_.erase(interface);
}

void FirewallManager::OnFirewallServiceAppeared() {
  LOG(INFO) << __func__;
  RequestAllPortsAccess();
}

void FirewallManager::OnFirewallServiceVanished() {
  // Nothing need to be done.
  LOG(INFO) << __func__;
}

void FirewallManager::RequestAllPortsAccess() {
  // Request access to DHCP port for all specified interfaces.
  for (const auto& dhcp_interface : dhcp_access_interfaces_) {
    firewall_proxy_->RequestUdpPortAccess(dhcp_interface, kDhcpServerPort);
  }
}

}  // namespace apmanager
