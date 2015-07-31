// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/permission_broker_proxy.h"

#include <string>
#include <vector>

#include <chromeos/dbus/service_constants.h>

#include "shill/logging.h"

namespace shill {
// static
const int PermissionBrokerProxy::kInvalidHandle = -1;

PermissionBrokerProxy::PermissionBrokerProxy(DBus::Connection* connection)
    : proxy_(connection),
      lifeline_read_fd_(kInvalidHandle),
      lifeline_write_fd_(kInvalidHandle) {}

PermissionBrokerProxy::~PermissionBrokerProxy() {}

bool PermissionBrokerProxy::RequestVpnSetup(
    const std::vector<std::string>& user_names,
    const std::string& interface) {
  if (lifeline_read_fd_ != kInvalidHandle ||
      lifeline_write_fd_ != kInvalidHandle) {
    LOG(ERROR) << "Already setup?";
    return false;
  }

  int fds[2];
  if (pipe(fds) != 0) {
    LOG(ERROR) << "Failed to create lifeline pipe";
    return false;
  }
  lifeline_read_fd_ = fds[0];
  lifeline_write_fd_ = fds[1];

  DBus::FileDescriptor dbus_fd(lifeline_read_fd_);
  bool return_value = false;
  try {
    return_value = proxy_.RequestVpnSetup(user_names, interface, dbus_fd);
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
  }
  return return_value;
}

bool PermissionBrokerProxy::RemoveVpnSetup() {
  bool return_value = true;
  if (lifeline_read_fd_ != kInvalidHandle &&
      lifeline_write_fd_ != kInvalidHandle) {
    close(lifeline_read_fd_);
    close(lifeline_write_fd_);
    lifeline_read_fd_ = kInvalidHandle;
    lifeline_write_fd_ = kInvalidHandle;
    try {
      return_value = proxy_.RemoveVpnSetup();
    } catch (const DBus::Error& e) {
      return_value = false;
      LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    }
  }
  return return_value;
}

PermissionBrokerProxy::Proxy::Proxy(DBus::Connection* connection)
    : DBus::ObjectProxy(*connection,
                        permission_broker::kPermissionBrokerServicePath,
                        permission_broker::kPermissionBrokerServiceName) {}

PermissionBrokerProxy::Proxy::~Proxy() {}

}  // namespace shill
