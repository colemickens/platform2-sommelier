// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/firewall_manager.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
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

void FirewallManager::Start(const scoped_refptr<dbus::Bus>& bus) {
  CHECK(!permission_broker_proxy_) << "Already started";

  if (!SetupLifelinePipe()) {
    return;
  }

  permission_broker_proxy_.reset(
      new org::chromium::PermissionBrokerProxy(
          bus,
          permission_broker::kPermissionBrokerServiceName));

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
  AddFirewallRules();
}

void FirewallManager::OnServiceNameChanged(const string& old_owner,
                                           const string& new_owner) {
  LOG(INFO) << "FirewallManager::OnServiceNameChanged old " << old_owner
            << " new " << new_owner;
  // Nothing to be done if no owner is attached to the proxy service.
  if (new_owner.empty()) {
    return;
  }
  AddFirewallRules();
}

void FirewallManager::AddFirewallRules() {
  AddUdpPortRule(kDhcpServerPort);
}

void FirewallManager::AddUdpPortRule(uint16_t port) {
  bool allowed = false;
  // Pass the read end of the pipe to permission_broker, for it to monitor this
  // process.
  dbus::FileDescriptor fd(lifeline_read_fd_);
  fd.CheckValidity();
  chromeos::ErrorPtr error;
  if (!permission_broker_proxy_->RequestUdpPortAccess(port,
                                                      fd,
                                                      &allowed,
                                                      &error)) {
    LOG(ERROR) << "Failed request UDP port access: "
               << error->GetCode() << " " << error->GetMessage();
    return;
  }
  if (!allowed) {
    LOG(ERROR) << "Access request for UDP port " << port << " is denied";
  }
  LOG(INFO) << "Added UDP port " << port;
}

}  // namespace apmanager
