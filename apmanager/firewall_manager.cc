// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/firewall_manager.h"

#include <base/bind.h>
#include <chromeos/errors/error.h>

using std::string;

namespace apmanager {

namespace {

const uint16_t kDhcpServerPort = 67;
const int kInvalidFd = -1;

}  // namespace

FirewallManager::FirewallManager()
    : lifeline_read_fd_(kInvalidFd),
      lifeline_write_fd_(kInvalidFd) {}

FirewallManager::~FirewallManager() {
  if (lifeline_read_fd_ != kInvalidFd) {
    close(lifeline_read_fd_);
    close(lifeline_write_fd_);
  }
}

void FirewallManager::Init(const scoped_refptr<dbus::Bus>& bus) {
  CHECK(!permission_broker_proxy_) << "Already started";

  if (!SetupLifelinePipe()) {
    return;
  }

  permission_broker_proxy_.reset(new org::chromium::PermissionBrokerProxy(bus));

  // This will connect the name owner changed signal in DBus object proxy,
  // The callback will be invoked as soon as service is avalilable. and will
  // be cleared after it is invoked. So this will be an one time callback.
  permission_broker_proxy_->GetObjectProxy()->WaitForServiceToBeAvailable(
      base::Bind(&FirewallManager::OnServiceAvailable, base::Unretained(this)));

  // This will continuously monitor the name owner of the service. However,
  // it does not connect the name owner changed signal in DBus object proxy
  // for some reason. In order to connect the name owner changed signal,
  // either WaitForServiceToBeAvaiable or ConnectToSignal need to be invoked.
  // Since we're not interested in any signals from the proxy,
  // WaitForServiceToBeAvailable is used.
  permission_broker_proxy_->GetObjectProxy()->SetNameOwnerChangedCallback(
      base::Bind(&FirewallManager::OnServiceNameChanged,
                 base::Unretained(this)));
}

void FirewallManager::RequestDHCPPortAccess(const std::string& interface) {
  CHECK(permission_broker_proxy_) << "Proxy not initialized yet";
  if (dhcp_access_interfaces_.find(interface) !=
      dhcp_access_interfaces_.end()) {
    LOG(ERROR) << "DHCP access already requested for interface: " << interface;
    return;
  }
  RequestUdpPortAccess(interface, kDhcpServerPort);
  dhcp_access_interfaces_.insert(interface);
}

void FirewallManager::ReleaseDHCPPortAccess(const std::string& interface) {
  CHECK(permission_broker_proxy_) << "Proxy not initialized yet";
  if (dhcp_access_interfaces_.find(interface) ==
      dhcp_access_interfaces_.end()) {
    LOG(ERROR) << "DHCP access has not been requested for interface: "
               << interface;
    return;
  }
  ReleaseUdpPortAccess(interface, kDhcpServerPort);
  dhcp_access_interfaces_.erase(interface);
}

bool FirewallManager::SetupLifelinePipe() {
  if (lifeline_read_fd_ != kInvalidFd) {
    LOG(ERROR) << "Lifeline pipe already created";
    return false;
  }

  // Setup lifeline pipe.
  int fds[2];
  if (pipe(fds) != 0) {
    PLOG(ERROR) << "Failed to create lifeline pipe";
    return false;
  }
  lifeline_read_fd_ = fds[0];
  lifeline_write_fd_ = fds[1];

  return true;
}

void FirewallManager::OnServiceAvailable(bool service_available) {
  LOG(INFO) << "FirewallManager::OnServiceAvailabe " << service_available;
  // Nothing to be done if proxy service is not available.
  if (!service_available) {
    return;
  }
  RequestAllPortsAccess();
}

void FirewallManager::OnServiceNameChanged(const string& old_owner,
                                           const string& new_owner) {
  LOG(INFO) << "FirewallManager::OnServiceNameChanged old " << old_owner
            << " new " << new_owner;
  // Nothing to be done if no owner is attached to the proxy service.
  if (new_owner.empty()) {
    return;
  }
  RequestAllPortsAccess();
}

void FirewallManager::RequestAllPortsAccess() {
  // Request access to DHCP port for all specified interfaces.
  for (const auto& dhcp_interface : dhcp_access_interfaces_) {
    RequestUdpPortAccess(dhcp_interface, kDhcpServerPort);
  }
}

void FirewallManager::RequestUdpPortAccess(const string& interface,
                                           uint16_t port) {
  bool allowed = false;
  // Pass the read end of the pipe to permission_broker, for it to monitor this
  // process.
  dbus::FileDescriptor fd(lifeline_read_fd_);
  fd.CheckValidity();
  chromeos::ErrorPtr error;
  if (!permission_broker_proxy_->RequestUdpPortAccess(port,
                                                      interface,
                                                      fd,
                                                      &allowed,
                                                      &error)) {
    LOG(ERROR) << "Failed to request UDP port access: "
               << error->GetCode() << " " << error->GetMessage();
    return;
  }
  if (!allowed) {
    LOG(ERROR) << "Access request for UDP port " << port
               << " on interface " << interface << " is denied";
    return;
  }
  LOG(INFO) << "Access granted for UDP port " << port
            << " on interface " << interface;;
}

void FirewallManager::ReleaseUdpPortAccess(const string& interface,
                                           uint16_t port) {
  chromeos::ErrorPtr error;
  bool success;
  if (!permission_broker_proxy_->ReleaseUdpPort(port,
                                                interface,
                                                &success,
                                                &error)) {
    LOG(ERROR) << "Failed to release UDP port access: "
               << error->GetCode() << " " << error->GetMessage();
    return;
  }
  if (!success) {
    LOG(ERROR) << "Release request for UDP port " << port
               << " on interface " << interface << " is denied";
    return;
  }
  LOG(INFO) << "Access released for UDP port " << port
            << " on interface " << interface;
}

}  // namespace apmanager
