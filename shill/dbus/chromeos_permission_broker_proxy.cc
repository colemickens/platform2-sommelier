// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_permission_broker_proxy.h"

#include <string>
#include <vector>

#include "shill/logging.h"

namespace shill {

// static
const int ChromeosPermissionBrokerProxy::kInvalidHandle = -1;

ChromeosPermissionBrokerProxy::ChromeosPermissionBrokerProxy(
    const scoped_refptr<dbus::Bus>& bus)
    : proxy_(new org::chromium::PermissionBrokerProxy(bus)),
      lifeline_read_fd_(kInvalidHandle),
      lifeline_write_fd_(kInvalidHandle) {
  // TODO(zqiu): register handler for service name owner changes, to
  // automatically re-request VPN setup when permission broker is restarted.
}

ChromeosPermissionBrokerProxy::~ChromeosPermissionBrokerProxy() {}

bool ChromeosPermissionBrokerProxy::RequestVpnSetup(
    const std::vector<std::string>& user_names,
    const std::string& interface) {
  if (lifeline_read_fd_ != kInvalidHandle ||
      lifeline_write_fd_ != kInvalidHandle) {
    LOG(ERROR) << "Already setup?";
    return false;
  }

  // TODO(zqiu): move pipe creation/cleanup to the constructor and destructor.
  // No need to recreate pipe for each request.
  int fds[2];
  if (pipe(fds) != 0) {
    LOG(ERROR) << "Failed to create lifeline pipe";
    return false;
  }
  lifeline_read_fd_ = fds[0];
  lifeline_write_fd_ = fds[1];

  dbus::FileDescriptor dbus_fd(lifeline_read_fd_);
  chromeos::ErrorPtr error;
  bool success = false;
  if (!proxy_->RequestVpnSetup(
      user_names, interface, dbus_fd, &success, &error)) {
    LOG(ERROR) << "Failed to request VPN setup: " << error->GetCode()
               << " " << error->GetMessage();
  }
  return success;
}

bool ChromeosPermissionBrokerProxy::RemoveVpnSetup() {
  if (lifeline_read_fd_ == kInvalidHandle &&
      lifeline_write_fd_ == kInvalidHandle) {
    return true;
  }

  close(lifeline_read_fd_);
  close(lifeline_write_fd_);
  lifeline_read_fd_ = kInvalidHandle;
  lifeline_write_fd_ = kInvalidHandle;
  chromeos::ErrorPtr error;
  bool success = false;
  if (!proxy_->RemoveVpnSetup(&success, &error)) {
    LOG(ERROR) << "Failed to remove VPN setup: " << error->GetCode()
               << " " << error->GetMessage();
  }
  return success;
}

}  // namespace shill
